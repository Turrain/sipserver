//
//  SipUS - DB
//
#include <unordered_set>
#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <deps/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <sys/mman.h>
#include <unistd.h>

using json = nlohmann::json;

struct Value;
using Array = std::vector<Value>;
using Object = std::unordered_map<std::string, Value>;
struct Value {
    // The variant now holds a dedicated null type (std::monostate) plus other alternatives.
    std::variant<
        std::monostate,
        std::string,
        int64_t,
        double,
        bool,
        Array,
        Object>
        data;

    // Direct constructors remain mostly the same.
    explicit Value(int n) : Value(static_cast<int64_t>(n)) { }
    explicit Value(Object obj) : data(std::move(obj)) { }
    explicit Value(Array arr) : data(std::move(arr)) { }
    explicit Value(std::string s) : data(std::move(s)) { }
    explicit Value(const char *s) : Value(std::string(s)) { }
    explicit Value(int64_t n) : data(n) { }
    explicit Value(double d) : data(d) { }
    explicit Value(bool b) : data(b) { }
    explicit Value(std::monostate nm = {}) : data(nm) { }

    // Improved JSON constructor:
    // Instead of using a try-catch, we now test explicitly whether the number is an integer or float.
    explicit Value(const json &j) {
        if (j.is_null()) {
            data = std::monostate{};
        } else if (j.is_string()) {
            data = j.get<std::string>();
        } else if (j.is_number_integer()) {
            data = j.get<int64_t>();
        } else if (j.is_number_float()) {
            data = j.get<double>();
        } else if (j.is_boolean()) {
            data = j.get<bool>();
        } else if (j.is_array()) {
            Array arr;
            arr.reserve(j.size());
            for (const auto &elem: j) {
                arr.emplace_back(Value(elem));
            }
            data = std::move(arr);
        } else if (j.is_object()) {
            Object obj;
            // Note: if using C++20 you can use obj.reserve(j.size());
            for (const auto &[key, val]: j.items()) {
                obj.emplace(key, Value(val));
            }
            data = std::move(obj);
        } else {
            throw std::runtime_error("Unsupported JSON type");
        }
    }

    // JSON conversion remains unchanged.
    json toJson() const {
        struct Visitor {
            json operator()(std::monostate) const { return nullptr; }
            json operator()(const std::string &s) const { return s; }
            json operator()(int64_t n) const { return n; }
            json operator()(double d) const { return d; }
            json operator()(bool b) const { return b; }
            json operator()(const Array &arr) const {
                json j = json::array();
                for (const auto &v: arr) {
                    j.push_back(v.toJson());
                }
                return j;
            }
            json operator()(const Object &obj) const {
                json j = json::object();
                for (const auto &[k, v]: obj) {
                    j[k] = v.toJson();
                }
                return j;
            }
        };
        return std::visit(Visitor{}, data);
    }

    // Equality comparison (unchanged, but note that using std::visit here lets us compare arithmetic types even if their
    // underlying types differ)
    bool operator==(const Value &other) const noexcept {
        return std::visit([](const auto &a, const auto &b) {
            using A = std::decay_t<decltype(a)>;
            using B = std::decay_t<decltype(b)>;
            if constexpr (!std::is_same_v<A, B>) {
                if constexpr (std::is_arithmetic_v<A> && std::is_arithmetic_v<B>) {
                    return a == b;
                }
                return false;
            } else {
                if constexpr (std::is_same_v<A, Array>) {
                    return a.size() == b.size() &&
                           std::equal(a.begin(), a.end(), b.begin(), [](const Value &x, const Value &y) {
                               return x == y;
                           });
                } else if constexpr (std::is_same_v<A, Object>) {
                    if (a.size() != b.size())
                        return false;
                    for (const auto &[k, v]: a) {
                        auto it = b.find(k);
                        if (it == b.end() || !(v == it->second))
                            return false;
                    }
                    return true;
                }
                return a == b;
            }
        }, data, other.data);
    }
};

class Document {
public:
    Document() = default;

    explicit Document(const json &j) :
        data_(std::get<Object>(Value(j).data)) { }

    json toJson() const
    {
        json j = json::object();
        for (const auto &[key, value]: data_) {
            j[key] = value.toJson();
        }
        return j;
    }

    std::vector<uint8_t> toBinary() const
    {
        json j = this->toJson();
        return json::to_bson(j);
    }
    static Document fromBinary(const std::vector<uint8_t> &data)
    {
        return Document(json::from_bson(data));
    }

    bool eraseKey(const std::string &key) noexcept
    {
        return data_.erase(key) > 0;
    }

    std::vector<std::string> getKeys() const noexcept
    {
        std::vector<std::string> keys;
        keys.reserve(data_.size());
        for (const auto &[k, _]: data_) {
            keys.push_back(k);
        }
        return keys;
    }

    size_t size() const noexcept { return data_.size(); }
    bool empty() const noexcept { return data_.empty(); }

    void saveToFile(const std::string &filename) const
    {
        auto binary = this->toBinary();
        int fd = open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
        ftruncate(fd, binary.size());
        void *addr = mmap(nullptr, binary.size(), PROT_WRITE, MAP_SHARED, fd, 0);
        memcpy(addr, binary.data(), binary.size());
        munmap(addr, binary.size());
        close(fd);
    }

    static Document loadFromFile(const std::string &filename)
    {
        int fd = open(filename.c_str(), O_RDONLY);
        size_t size = lseek(fd, 0, SEEK_END);
        void *addr = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
        std::vector<uint8_t> data(size);
        memcpy(data.data(), addr, size);
        munmap(addr, size);
        close(fd);
        return fromBinary(data);
    }
    void insert(std::string key, Value value)
    {
        data_.emplace(std::move(key), std::move(value));
    }

    std::optional<std::reference_wrapper<const Value>> get(const std::string &key) const noexcept
    {
        auto it = data_.find(key);
        if (it != data_.end())
            return std::cref(it->second);
        return std::nullopt;
    }

    bool contains(const std::string &key) const noexcept
    {
        return data_.count(key) > 0;
    }

private:
    Object data_;
};

class Table {
public:
    // Callback types
    using BeforeModifyCallback = std::function<bool(const std::string &, const Document &, Document &)>;
    using AfterModifyCallback = std::function<void(const std::string &, const Document &)>;
    using BeforeDeleteCallback = std::function<bool(const std::string &, const Document &)>;
    using AfterDeleteCallback = std::function<void(const std::string &, const Document &)>;
    using TransactionCallback = std::function<void(const Table &)>;

    // Callback registration
    void registerBeforeInsert(BeforeModifyCallback cb) noexcept
    {
        before_insert_.emplace_back(std::move(cb));
    }

    void registerAfterInsert(AfterModifyCallback cb) noexcept
    {
        after_insert_.emplace_back(std::move(cb));
    }

    void registerBeforeUpdate(BeforeModifyCallback cb) noexcept
    {
        before_update_.emplace_back(std::move(cb));
    }

    void registerAfterUpdate(AfterModifyCallback cb) noexcept
    {
        after_update_.emplace_back(std::move(cb));
    }

    void registerBeforeDelete(BeforeDeleteCallback cb) noexcept
    {
        before_delete_.emplace_back(std::move(cb));
    }

    void registerAfterDelete(AfterDeleteCallback cb) noexcept
    {
        after_delete_.emplace_back(std::move(cb));
    }

    using const_iterator = typename std::unordered_map<std::string, Document>::const_iterator;

    const_iterator begin() const noexcept { return documents_.begin(); }
    const_iterator end() const noexcept { return documents_.end(); }

    void insertDocument(std::string id, Document doc)
    {
        if (!documents_.emplace(std::move(id), std::move(doc)).second) {
            throw std::runtime_error("Document exists");
        }
    }
    template<typename Predicate>
    size_t deleteDocuments(Predicate &&pred)
    {
        size_t count = 0;
        auto it = documents_.begin();
        while (it != documents_.end()) {
            if (pred(it->second)) {
                it = documents_.erase(it);
                ++count;
            } else {
                ++it;
            }
        }
        return count;
    }

    template<typename Predicate, typename UpdateFn>
    size_t updateDocuments(Predicate &&pred, UpdateFn &&updateFn)
    {
        size_t count = 0;
        for (auto &[id, doc]: documents_) {
            if (pred(doc)) {
                updateFn(doc);
                ++count;
            }
        }
        return count;
    }

    template<typename Predicate>
    size_t count(Predicate &&pred) const noexcept
    {
        return std::count_if(documents_.begin(), documents_.end(),
            [&pred](const auto &entry) { return pred(entry.second); });
    }

    template<typename T = Value>
    std::vector<T> distinctValues(const std::string &key) const
    {
        std::unordered_set<T> unique_values;
        for (const auto &[_, doc]: documents_) {
            if (auto val = doc.get(key)) {
                try {
                    unique_values.insert(std::get<T>(val->get().data));
                } catch (const std::bad_variant_access &) {
                    // Type mismatch, skip this value
                }
            }
        }
        return { unique_values.begin(), unique_values.end() };
    }

    template<typename Predicate>
    bool exists(Predicate &&pred) const noexcept
    {
        return std::any_of(documents_.begin(), documents_.end(),
            [&pred](const auto &entry) { return pred(entry.second); });
    }
    const Document &getDocument(const std::string &id) const
    {
        auto it = documents_.find(id);
        if (it == documents_.end())
            throw std::runtime_error("Document not found");
        return it->second;
    }

    void saveToDirectory(const std::string &path) const
    {
        namespace fs = std::filesystem;
        fs::create_directories(path);

        for (const auto &[id, doc]: documents_) {
            std::string filepath = path + "/" + id + ".bson";
            doc.saveToFile(filepath);
        }
    }

    void loadFromDirectory(const std::string &path)
    {
        namespace fs = std::filesystem;
        documents_.clear();

        for (const auto &entry: fs::directory_iterator(path)) {
            if (entry.path().extension() == ".bson") {
                std::string id = entry.path().stem();
                Document doc = Document::loadFromFile(entry.path());
                documents_.emplace(std::move(id), std::move(doc));
            }
        }
    }

     bool tryInsertDocument(std::string id, Document doc) noexcept
    {
        TransactionGuard guard(*this);
        
        // Run before-insert callbacks
        for (const auto& cb : before_insert_) {
            if (!cb(id, Document{}, doc)) {  // Empty doc for 'old' state
                return false;
            }
        }

        auto [it, inserted] = documents_.try_emplace(std::move(id), std::move(doc));
        if (!inserted) return false;

        // Run after-insert callbacks
        for (const auto& cb : after_insert_) {
            cb(it->first, it->second);
        }

        guard.commit();
        return true;
    }

    bool tryDeleteDocument(const std::string& id) noexcept
    {
        TransactionGuard guard(*this);
        auto it = documents_.find(id);
        if (it == documents_.end()) return false;

        // Run before-delete callbacks
        for (const auto& cb : before_delete_) {
            if (!cb(id, it->second)) {
                return false;
            }
        }

        Document deleted_doc = std::move(it->second);
        documents_.erase(it);

        // Run after-delete callbacks
        for (const auto& cb : after_delete_) {
            cb(id, deleted_doc);
        }

        guard.commit();
        return true;
    }

     bool tryUpdateDocument(std::string id, Document doc) noexcept
    {
        TransactionGuard guard(*this);
        auto it = documents_.find(id);
        if (it == documents_.end()) return false;

        Document old_doc = it->second;
        
        // Run before-update callbacks
        for (const auto& cb : before_update_) {
            if (!cb(id, old_doc, doc)) {
                return false;
            }
        }

        it->second = std::move(doc);

        // Run after-update callbacks
        for (const auto& cb : after_update_) {
            cb(id, it->second);
        }

        guard.commit();
        return true;
    }

    size_t insertDocuments(const std::vector<std::pair<std::string, Document>> &docs) noexcept
    {
        size_t count = 0;
        for (const auto &[id, doc]: docs) {
            if (tryInsertDocument(id, doc)) {
                ++count;
            }
        }
        return count;
    }

    size_t deleteDocuments(const std::vector<std::string> &ids) noexcept
    {
        size_t count = 0;
        for (const auto &id: ids) {
            count += tryDeleteDocument(id);
        }
        return count;
    }

    std::vector<std::reference_wrapper<const Document>> getAllDocuments() const noexcept
    {
        std::vector<std::reference_wrapper<const Document>> result;
        result.reserve(documents_.size());
        for (const auto &[_, doc]: documents_) {
            result.emplace_back(std::cref(doc));
        }
        return result;
    }

    std::vector<std::string> getAllDocumentIds() const noexcept
    {
        std::vector<std::string> ids;
        ids.reserve(documents_.size());
        for (const auto &[id, _]: documents_) {
            ids.push_back(id);
        }
        return ids;
    }

    void clear() noexcept { documents_.clear(); }
    size_t size() const noexcept { return documents_.size(); }
    bool empty() const noexcept { return documents_.empty(); }
    bool containsDocument(const std::string &id) const noexcept
    {
        return documents_.find(id) != documents_.end();
    }

    template<typename Predicate>
    std::vector<std::reference_wrapper<const Document>> queryDocuments(Predicate &&pred) const
    {
        std::vector<std::reference_wrapper<const Document>> results;
        for (const auto &[_, doc]: documents_) {
            if (pred(doc)) {
                results.emplace_back(std::cref(doc));
            }
        }
        return results;
    }

    std::vector<std::reference_wrapper<const Document>> findDocumentsByKey(const std::string &key) const
    {
        return queryDocuments([&key](const Document &doc) {
            return doc.contains(key);
        });
    }

    template<typename T>
    std::vector<std::reference_wrapper<const Document>> findDocumentsByValue(
        const std::string &key, const T &value) const
    {
        Value v(value);
        return queryDocuments([&key, &v](const Document &doc) {
            auto opt = doc.get(key);
            return opt && opt->get() == v;
        });
    }

private:
    class TransactionGuard {
    public:
        explicit TransactionGuard(Table& table) : table_(table) {}
        
        void commit() noexcept {
            committed_ = true;
            for (const auto& cb : table_.on_commit_) {
                cb(table_);
            }
        }

        ~TransactionGuard() {
            if (!committed_) {
                for (const auto& cb : table_.on_rollback_) {
                    cb(table_);
                }
            }
        }

    private:
        Table& table_;
        bool committed_ = false;
    };

    std::unordered_map<std::string, Document> documents_;
    std::vector<BeforeModifyCallback> before_insert_;
    std::vector<AfterModifyCallback> after_insert_;
    std::vector<BeforeModifyCallback> before_update_;
    std::vector<AfterModifyCallback> after_update_;
    std::vector<BeforeDeleteCallback> before_delete_;
    std::vector<AfterDeleteCallback> after_delete_;
    std::vector<TransactionCallback> on_commit_;
    std::vector<TransactionCallback> on_rollback_;
};

class InMemoryDatabase {
public:
    void saveToDirectory(const std::string &path) const
    {
        namespace fs = std::filesystem;
        fs::create_directories(path);

        for (const auto &[name, table]: tables_) {
            std::string tablePath = path + "/" + name;
            table.saveToDirectory(tablePath);
        }
    }

    void loadFromDirectory(const std::string &path)
    {
        namespace fs = std::filesystem;
        tables_.clear();

        for (const auto &entry: fs::directory_iterator(path)) {
            if (entry.is_directory()) {
                std::string tableName = entry.path().filename();
                Table table;
                table.loadFromDirectory(entry.path());
                tables_.emplace(tableName, std::move(table));
            }
        }
    }

    void saveToFile(const std::string &filename) const
    {
        json db;
        for (const auto &[tableName, table]: tables_) {
            json tableJson;
            // Iterate directly through the table's internal document map
            for (const auto &[docId, doc]: table) {
                tableJson[docId] = doc.toJson();
            }
            db[tableName] = tableJson;
        }
        auto binary = json::to_bson(db);
        std::ofstream file(filename, std::ios::binary);
        file.write(reinterpret_cast<const char *>(binary.data()), binary.size());
    }

    void loadFromFile(const std::string &filename)
    {
        std::ifstream file(filename, std::ios::binary);
        std::vector<uint8_t> binary(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());
        json db = json::from_bson(binary);
        tables_.clear();
        for (auto &[tableName, tableJson]: db.items()) {
            Table table;
            for (auto &[docId, docJson]: tableJson.items()) {
                Document doc(docJson);
                table.insertDocument(docId, std::move(doc));
            }
            tables_.emplace(tableName, std::move(table));
        }
    }

    Table &createTable(const std::string &name)
    {
        auto [it, inserted] = tables_.try_emplace(name);
        if (!inserted) {
            throw std::runtime_error("Table already exists");
        }
        return it->second;
    }

    Table &getTable(const std::string &name)
    {
        auto it = tables_.find(name);
        if (it == tables_.end()) {
            throw std::runtime_error("Table not found");
        }
        return it->second;
    }

    const Table &getTable(const std::string &name) const
    {
        auto it = tables_.find(name);
        if (it == tables_.end()) {
            throw std::runtime_error("Table not found");
        }
        return it->second;
    }

    bool hasTable(const std::string &name) const noexcept
    {
        return tables_.find(name) != tables_.end();
    }

    bool empty() const noexcept { return tables_.empty(); }

private:
    std::unordered_map<std::string, Table> tables_;
};







