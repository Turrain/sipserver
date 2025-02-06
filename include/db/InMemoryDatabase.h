//
//  SipUS - DB (Revised)
//

#include <unordered_set>
#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <fstream>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <sys/mman.h>
#include <unistd.h>

// Include your JSON library header here:
#include <deps/json.hpp>
using json = nlohmann::json;

// ------------------------------------------------------------------
// Forward declaration and definitions for recursive types
// ------------------------------------------------------------------
struct Value;

// Define Array and Object to hold pointers to Value.
using Array  = std::vector<std::unique_ptr<Value>>;
using Object = std::unordered_map<std::string, std::unique_ptr<Value>>;

// ------------------------------------------------------------------
// Value: A variant type that supports recursion via indirection
// ------------------------------------------------------------------
struct Value {
    // The variant holds basic types plus Array and Object.
    std::variant<
        std::monostate,
        std::string,
        int64_t,
        double,
        bool,
        Array,
        Object
    > data;

    // ----- Constructors for basic types -----
    explicit Value(int n) : Value(static_cast<int64_t>(n)) { }
    explicit Value(const char* s) : Value(std::string(s)) { }
    explicit Value(std::string s) : data(std::move(s)) { }
    explicit Value(int64_t n) : data(n) { }
    explicit Value(double d) : data(d) { }
    explicit Value(bool b) : data(b) { }
    explicit Value(std::monostate m = {}) : data(m) { }

    // ----- JSON Constructor -----
    explicit Value(const json &j) {
        if (j.is_null()) {
            data = std::monostate{};
        }
        else if (j.is_string()) {
            data = j.get<std::string>();
        }
        else if (j.is_number_integer()) {
            data = j.get<int64_t>();
        }
        else if (j.is_number_float()) {
            data = j.get<double>();
        }
        else if (j.is_boolean()) {
            data = j.get<bool>();
        }
        else if (j.is_array()) {
            Array arr;
            arr.reserve(j.size());
            for (const auto &elem: j) {
                arr.push_back(std::make_unique<Value>(elem));
            }
            data = std::move(arr);
        }
        else if (j.is_object()) {
            Object obj;
            for (const auto &[key, val]: j.items()) {
                obj.emplace(key, std::make_unique<Value>(val));
            }
            data = std::move(obj);
        }
        else {
            throw std::runtime_error("Unsupported JSON type");
        }
    }

    // ----- Deep Copy Support -----
    // Since Array and Object hold unique_ptr, we must copy them manually.
     Value(const Value &other) {
        data = std::visit([](const auto &arg) -> std::variant<std::monostate, std::string, int64_t, double, bool, Array, Object> {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, Array>) {
                Array newArr;
                newArr.reserve(arg.size());
                for (const auto &ptr : arg) {
                    newArr.push_back(std::make_unique<Value>(*ptr));
                }
                return newArr;
            }
            else if constexpr (std::is_same_v<T, Object>) {
                Object newObj;
                for (const auto &pair : arg) {
                    newObj.emplace(pair.first, std::make_unique<Value>(*pair.second));
                }
                return newObj;
            }
            else {
                return arg;
            }
        }, other.data);
    }

    Value &operator=(const Value &other) {
        if (this != &other) {
            data = std::visit([](const auto &arg) -> std::variant<std::monostate, std::string, int64_t, double, bool, Array, Object> {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, Array>) {
                    Array newArr;
                    newArr.reserve(arg.size());
                    for (const auto &ptr : arg) {
                        newArr.push_back(std::make_unique<Value>(*ptr));
                    }
                    return newArr;
                }
                else if constexpr (std::is_same_v<T, Object>) {
                    Object newObj;
                    for (const auto &pair : arg) {
                        newObj.emplace(pair.first, std::make_unique<Value>(*pair.second));
                    }
                    return newObj;
                }
                else {
                    return arg;
                }
            }, other.data);
        }
        return *this;
    }

    Value(Value &&) = default;
    Value &operator=(Value &&) = default;

    // ----- JSON Conversion -----
    json toJson() const {
        struct Visitor {
            json operator()(std::monostate) const { return nullptr; }
            json operator()(const std::string &s) const { return s; }
            json operator()(int64_t n) const { return n; }
            json operator()(double d) const { return d; }
            json operator()(bool b) const { return b; }
            json operator()(const Array &arr) const {
                json j = json::array();
                for (const auto &ptr : arr) {
                    j.push_back(ptr->toJson());
                }
                return j;
            }
            json operator()(const Object &obj) const {
                json j = json::object();
                for (const auto &pair : obj) {
                    j[pair.first] = pair.second->toJson();
                }
                return j;
            }
        };
        return std::visit(Visitor{}, data);
    }

    // ----- Equality Comparison -----
    bool operator==(const Value &other) const noexcept {
        return std::visit([](const auto &a, const auto &b) -> bool {
            using A = std::decay_t<decltype(a)>;
            using B = std::decay_t<decltype(b)>;
            if constexpr (!std::is_same_v<A, B>) {
                if constexpr (std::is_arithmetic_v<A> && std::is_arithmetic_v<B>) {
                    return a == b;
                }
                return false;
            }
            else {
                if constexpr (std::is_same_v<A, Array>) {
                    if (a.size() != b.size()) return false;
                    for (size_t i = 0; i < a.size(); ++i)
                        if (!(*a[i] == *b[i]))
                            return false;
                    return true;
                }
                else if constexpr (std::is_same_v<A, Object>) {
                    if (a.size() != b.size()) return false;
                    for (const auto &pair: a) {
                        auto it = b.find(pair.first);
                        if (it == b.end() || !(*pair.second == *it->second))
                            return false;
                    }
                    return true;
                }
                else {
                    return a == b;
                }
            }
        }, data, other.data);
    }
};

// ------------------------------------------------------------------
// Document: Wraps an Object (i.e. a JSON object) and provides file I/O.
// ------------------------------------------------------------------
class Document {
public:
    Document() = default;

    // Construct a Document from a JSON object.
    // Throws if j is not an object.
    explicit Document(const json &j) {
        if (!j.is_object())
            throw std::runtime_error("Document JSON must be an object");
        for (const auto &[key, val] : j.items()) {
            data_.emplace(key, std::make_unique<Value>(val));
        }
    }

    // Copy constructor (deep copy)
    Document(const Document &other) {
        for (const auto &pair : other.data_) {
            data_.emplace(pair.first, std::make_unique<Value>(*pair.second));
        }
    }

    Document &operator=(const Document &other) {
        if (this != &other) {
            data_.clear();
            for (const auto &pair : other.data_) {
                data_.emplace(pair.first, std::make_unique<Value>(*pair.second));
            }
        }
        return *this;
    }

    Document(Document &&) = default;
    Document &operator=(Document &&) = default;

    // Convert the Document back to JSON.
    json toJson() const {
        json j = json::object();
        for (const auto &[key, ptr] : data_) {
            j[key] = ptr->toJson();
        }
        return j;
    }

    // Convert to BSON binary.
    std::vector<uint8_t> toBinary() const {
        json j = toJson();
        return json::to_bson(j);
    }

    // Create a Document from BSON binary.
    static Document fromBinary(const std::vector<uint8_t> &data) {
        json j = json::from_bson(data);
        return Document(j);
    }

    // Save the Document to a file.
    void saveToFile(const std::string &filename) const {
        auto binary = toBinary();
        int fd = open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
        if (fd < 0)
            throw std::runtime_error("Failed to open file for writing: " + filename);
        // Resize the file.
        if (ftruncate(fd, binary.size()) != 0)
            throw std::runtime_error("Failed to truncate file: " + filename);
        void *addr = mmap(nullptr, binary.size(), PROT_WRITE, MAP_SHARED, fd, 0);
        if (addr == MAP_FAILED)
            throw std::runtime_error("Failed to mmap file: " + filename);
        std::memcpy(addr, binary.data(), binary.size());
        munmap(addr, binary.size());
        close(fd);
    }

    // Load a Document from a file.
    static Document loadFromFile(const std::string &filename) {
        int fd = open(filename.c_str(), O_RDONLY);
        if (fd < 0)
            throw std::runtime_error("Failed to open file for reading: " + filename);
        size_t size = lseek(fd, 0, SEEK_END);
        lseek(fd, 0, SEEK_SET);
        void *addr = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (addr == MAP_FAILED)
            throw std::runtime_error("Failed to mmap file: " + filename);
        std::vector<uint8_t> data(size);
        std::memcpy(data.data(), addr, size);
        munmap(addr, size);
        close(fd);
        return fromBinary(data);
    }

    // Insert a key/value pair into the document.
    // A deep copy is made.
    void insert(std::string key, const Value &value) {
        data_.emplace(std::move(key), std::make_unique<Value>(value));
    }

    // Retrieve a value by key. Returns nullopt if not found.
    std::optional<std::reference_wrapper<const Value>> get(const std::string &key) const noexcept {
        auto it = data_.find(key);
        if (it != data_.end())
            return std::cref(*(it->second));
        return std::nullopt;
    }

    // Erase a key from the document.
    bool eraseKey(const std::string &key) noexcept {
        return data_.erase(key) > 0;
    }

    // Get all keys in the document.
    std::vector<std::string> getKeys() const noexcept {
        std::vector<std::string> keys;
        keys.reserve(data_.size());
        for (const auto &pair : data_)
            keys.push_back(pair.first);
        return keys;
    }

    size_t size() const noexcept { return data_.size(); }
    bool empty() const noexcept { return data_.empty(); }

private:
    // The document stores an Object (i.e. key → unique_ptr<Value>).
    Object data_;
};

// ------------------------------------------------------------------
// Table: A collection of Documents with callback support
// ------------------------------------------------------------------
class Table {
public:
    // Callback types.
    using BeforeModifyCallback = std::function<bool(const std::string &, const Document &, Document &)>;
    using AfterModifyCallback  = std::function<void(const std::string &, const Document &)>;
    using BeforeDeleteCallback = std::function<bool(const std::string &, const Document &)>;
    using AfterDeleteCallback  = std::function<void(const std::string &, const Document &)>;
    using TransactionCallback  = std::function<void(const Table &)>;

    // Callback registration.
    void registerBeforeInsert(BeforeModifyCallback cb) { before_insert_.push_back(std::move(cb)); }
    void registerAfterInsert(AfterModifyCallback cb) { after_insert_.push_back(std::move(cb)); }
    void registerBeforeUpdate(BeforeModifyCallback cb) { before_update_.push_back(std::move(cb)); }
    void registerAfterUpdate(AfterModifyCallback cb) { after_update_.push_back(std::move(cb)); }
    void registerBeforeDelete(BeforeDeleteCallback cb) { before_delete_.push_back(std::move(cb)); }
    void registerAfterDelete(AfterDeleteCallback cb) { after_delete_.push_back(std::move(cb)); }

    // For iterating over documents.
    using const_iterator = typename std::unordered_map<std::string, Document>::const_iterator;
    const_iterator begin() const noexcept { return documents_.begin(); }
    const_iterator end() const noexcept { return documents_.end(); }

    // Insert a document. Throws if a document with the same id exists.
    void insertDocument(std::string id, Document doc) {
        if (!documents_.emplace(std::move(id), std::move(doc)).second) {
            throw std::runtime_error("Document exists");
        }
    }

    template<typename Predicate>
    size_t deleteDocuments(Predicate &&pred) {
        size_t count = 0;
        auto it = documents_.begin();
        while (it != documents_.end()) {
            if (pred(it->second)) {
                it = documents_.erase(it);
                ++count;
            }
            else {
                ++it;
            }
        }
        return count;
    }

    template<typename Predicate, typename UpdateFn>
    size_t updateDocuments(Predicate &&pred, UpdateFn &&updateFn) {
        size_t count = 0;
        for (auto &[id, doc] : documents_) {
            if (pred(doc)) {
                updateFn(doc);
                ++count;
            }
        }
        return count;
    }

    template<typename Predicate>
    size_t count(Predicate &&pred) const noexcept {
        return std::count_if(documents_.begin(), documents_.end(),
                             [&pred](const auto &entry) { return pred(entry.second); });
    }

    // Return the document with the given id, or throw if not found.
    const Document &getDocument(const std::string &id) const {
        auto it = documents_.find(id);
        if (it == documents_.end())
            throw std::runtime_error("Document not found");
        return it->second;
    }

    // Save all documents to a directory (one BSON file per document).
    void saveToDirectory(const std::string &path) const {
        namespace fs = std::filesystem;
        fs::create_directories(path);
        for (const auto &[id, doc] : documents_) {
            std::string filepath = path + "/" + id + ".bson";
            doc.saveToFile(filepath);
        }
    }

    // Load all BSON files from a directory into the table.
    void loadFromDirectory(const std::string &path) {
        namespace fs = std::filesystem;
        documents_.clear();
        for (const auto &entry : fs::directory_iterator(path)) {
            if (entry.path().extension() == ".bson") {
                std::string id = entry.path().stem();
                Document doc = Document::loadFromFile(entry.path());
                documents_.emplace(std::move(id), std::move(doc));
            }
        }
    }

    // These try* functions run callbacks before/after modifying a document.
    bool tryInsertDocument(std::string id, Document doc) {
        TransactionGuard guard(*this);
        for (const auto &cb : before_insert_) {
            if (!cb(id, Document{}, doc)) // Pass an empty Document as the “old” state.
                return false;
        }
        auto [it, inserted] = documents_.try_emplace(std::move(id), std::move(doc));
        if (!inserted) return false;
        for (const auto &cb : after_insert_) {
            cb(it->first, it->second);
        }
        guard.commit();
        return true;
    }

    bool tryDeleteDocument(const std::string &id) {
        TransactionGuard guard(*this);
        auto it = documents_.find(id);
        if (it == documents_.end()) return false;
        for (const auto &cb : before_delete_) {
            if (!cb(id, it->second))
                return false;
        }
        Document deleted_doc = std::move(it->second);
        documents_.erase(it);
        for (const auto &cb : after_delete_) {
            cb(id, deleted_doc);
        }
        guard.commit();
        return true;
    }

    bool tryUpdateDocument(std::string id, Document doc) {
        TransactionGuard guard(*this);
        auto it = documents_.find(id);
        if (it == documents_.end()) return false;
        Document old_doc = it->second;
        for (const auto &cb : before_update_) {
            if (!cb(id, old_doc, doc))
                return false;
        }
        it->second = std::move(doc);
        for (const auto &cb : after_update_) {
            cb(id, it->second);
        }
        guard.commit();
        return true;
    }

    // Other helper methods…
    std::vector<std::reference_wrapper<const Document>> getAllDocuments() const {
        std::vector<std::reference_wrapper<const Document>> result;
        result.reserve(documents_.size());
        for (const auto &[_, doc] : documents_)
            result.push_back(std::cref(doc));
        return result;
    }

    std::vector<std::string> getAllDocumentIds() const {
        std::vector<std::string> ids;
        ids.reserve(documents_.size());
        for (const auto &[id, _] : documents_)
            ids.push_back(id);
        return ids;
    }

    void clear() { documents_.clear(); }
    size_t size() const { return documents_.size(); }
    bool empty() const { return documents_.empty(); }
    bool containsDocument(const std::string &id) const {
        return documents_.find(id) != documents_.end();
    }

private:
    // A simple transaction guard that calls commit or rollback callbacks.
    class TransactionGuard {
    public:
        explicit TransactionGuard(Table &table) : table_(table) {}
        void commit() {
            committed_ = true;
            for (const auto &cb : table_.on_commit_) {
                cb(table_);
            }
        }
        ~TransactionGuard() {
            if (!committed_) {
                for (const auto &cb : table_.on_rollback_) {
                    cb(table_);
                }
            }
        }
    private:
        Table &table_;
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

// ------------------------------------------------------------------
// InMemoryDatabase: A collection of tables.
// ------------------------------------------------------------------
class InMemoryDatabase {
public:
    void clear() { tables_.clear(); }

    void saveToDirectory(const std::string &path) const {
        namespace fs = std::filesystem;
        fs::create_directories(path);
        for (const auto &[name, table] : tables_) {
            std::string tablePath = path + "/" + name;
            table.saveToDirectory(tablePath);
        }
    }

    void loadFromDirectory(const std::string &path) {
        namespace fs = std::filesystem;
        tables_.clear();
        for (const auto &entry : fs::directory_iterator(path)) {
            if (entry.is_directory()) {
                std::string tableName = entry.path().filename().string();
                Table table;
                table.loadFromDirectory(entry.path().string());
                tables_.emplace(tableName, std::move(table));
            }
        }
    }

    // Save the entire database as a BSON file.
    void saveToFile(const std::string &filename) const {
        json db;
        for (const auto &[tableName, table] : tables_) {
            json tableJson;
            for (const auto &[docId, doc] : table) {
                tableJson[docId] = doc.toJson();
            }
            db[tableName] = tableJson;
        }
        auto binary = json::to_bson(db);
        std::ofstream file(filename, std::ios::binary);
        file.write(reinterpret_cast<const char *>(binary.data()), binary.size());
    }

    // Load the entire database from a BSON file.
    void loadFromFile(const std::string &filename) {
        std::ifstream file(filename, std::ios::binary);
        std::vector<uint8_t> binary((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
        json db = json::from_bson(binary);
        tables_.clear();
        for (auto &[tableName, tableJson] : db.items()) {
            Table table;
            for (auto &[docId, docJson] : tableJson.items()) {
                Document doc(docJson);
                table.insertDocument(docId, std::move(doc));
            }
            tables_.emplace(tableName, std::move(table));
        }
    }

    // Create a table. Throws if the table already exists.
    Table &createTable(const std::string &name) {
        auto [it, inserted] = tables_.try_emplace(name);
        if (!inserted)
            throw std::runtime_error("Table already exists");
        return it->second;
    }

    Table &getTable(const std::string &name) {
        auto it = tables_.find(name);
        if (it == tables_.end())
            throw std::runtime_error("Table not found");
        return it->second;
    }

    const Table &getTable(const std::string &name) const {
        auto it = tables_.find(name);
        if (it == tables_.end())
            throw std::runtime_error("Table not found");
        return it->second;
    }

    bool hasTable(const std::string &name) const {
        return tables_.find(name) != tables_.end();
    }

    bool empty() const { return tables_.empty(); }

private:
    std::unordered_map<std::string, Table> tables_;
};
