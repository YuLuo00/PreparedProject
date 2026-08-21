#include "MetadataStore.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace coser {

MetadataStore::MetadataStore() = default;

MetadataStore::~MetadataStore() {
    Close();
}

bool MetadataStore::Open(const std::string& dbPath) {
    fs::path p(dbPath);
    if (p.has_parent_path()) {
        fs::create_directories(p.parent_path());
    }
    int rc = sqlite3_open(dbPath.c_str(), &db_);
    if (rc != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }
    Execute("PRAGMA journal_mode=WAL;");
    Execute("PRAGMA synchronous=NORMAL;");
    Execute("PRAGMA foreign_keys=ON;");
    return CreateTables();
}

void MetadataStore::Close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool MetadataStore::Execute(const std::string& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        lastError_ = errMsg ? errMsg : "unknown error";
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

bool MetadataStore::CreateTables() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS persons (
            person_id     INTEGER PRIMARY KEY AUTOINCREMENT,
            display_name  TEXT NOT NULL,
            category      TEXT,
            created_at    INTEGER NOT NULL DEFAULT (strftime('%s','now'))
        );

        CREATE TABLE IF NOT EXISTS roles (
            role_id       INTEGER PRIMARY KEY AUTOINCREMENT,
            display_name  TEXT NOT NULL UNIQUE,
            created_at    INTEGER NOT NULL DEFAULT (strftime('%s','now'))
        );

        CREATE TABLE IF NOT EXISTS images (
            image_id      INTEGER PRIMARY KEY AUTOINCREMENT,
            person_id     INTEGER REFERENCES persons(person_id),
            role_id       INTEGER REFERENCES roles(role_id),
            file_path     TEXT NOT NULL UNIQUE,
            md5           TEXT,
            width         INTEGER,
            height        INTEGER,
            format        TEXT,
            phash         INTEGER,
            ingest_status TEXT NOT NULL DEFAULT 'pending',
            fail_reason   TEXT,
            created_at    INTEGER NOT NULL DEFAULT (strftime('%s','now'))
        );
        CREATE INDEX IF NOT EXISTS idx_images_person ON images(person_id);
        CREATE INDEX IF NOT EXISTS idx_images_status ON images(ingest_status);

        CREATE TABLE IF NOT EXISTS face_embeddings (
            embedding_id  INTEGER PRIMARY KEY AUTOINCREMENT,
            image_id      INTEGER NOT NULL REFERENCES images(image_id),
            bbox_x REAL, bbox_y REAL, bbox_w REAL, bbox_h REAL,
            det_score     REAL,
            created_at    INTEGER NOT NULL DEFAULT (strftime('%s','now'))
        );
        CREATE INDEX IF NOT EXISTS idx_face_emb_image ON face_embeddings(image_id);

        CREATE TABLE IF NOT EXISTS clothing_embeddings (
            embedding_id   INTEGER PRIMARY KEY AUTOINCREMENT,
            image_id       INTEGER NOT NULL REFERENCES images(image_id),
            bbox_x REAL, bbox_y REAL, bbox_w REAL, bbox_h REAL,
            pose_keypoints BLOB,
            created_at     INTEGER NOT NULL DEFAULT (strftime('%s','now'))
        );
        CREATE INDEX IF NOT EXISTS idx_cloth_emb_image ON clothing_embeddings(image_id);
    )";
    if (!Execute(sql)) return false;

    // Databases created before role recognition do not have images.role_id.
    if (!ColumnExists("images", "role_id") &&
        !Execute("ALTER TABLE images ADD COLUMN role_id INTEGER REFERENCES roles(role_id);")) {
        return false;
    }
    if (!ColumnExists("images", "md5") && !Execute("ALTER TABLE images ADD COLUMN md5 TEXT;")) {
        return false;
    }
    return Execute("CREATE INDEX IF NOT EXISTS idx_images_role ON images(role_id);") &&
           Execute("CREATE UNIQUE INDEX IF NOT EXISTS idx_images_md5_unique "
                   "ON images(md5) WHERE md5 IS NOT NULL;");
}

bool MetadataStore::ColumnExists(const std::string& table, const std::string& column) {
    sqlite3_stmt* stmt = nullptr;
    std::string sql = "PRAGMA table_info(" + table + ")";
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        return false;
    }
    bool found = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (name && column == name) {
            found = true;
            break;
        }
    }
    sqlite3_finalize(stmt);
    return found;
}

int64_t MetadataStore::InsertPerson(const std::string& displayName, const std::string& category) {
    const char* sql = "INSERT INTO persons(display_name, category) VALUES(?,?)";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, displayName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, category.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        lastError_ = sqlite3_errmsg(db_);
        return -1;
    }
    return sqlite3_last_insert_rowid(db_);
}

int64_t MetadataStore::InsertRole(const std::string& displayName) {
    const char* insertSql = "INSERT OR IGNORE INTO roles(display_name) VALUES(?)";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, insertSql, -1, &stmt, nullptr) != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, displayName.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        lastError_ = sqlite3_errmsg(db_);
        return -1;
    }

    const char* selectSql = "SELECT role_id FROM roles WHERE display_name=?";
    if (sqlite3_prepare_v2(db_, selectSql, -1, &stmt, nullptr) != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, displayName.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        lastError_ = sqlite3_errmsg(db_);
        sqlite3_finalize(stmt);
        return -1;
    }
    int64_t roleId = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return roleId;
}

int64_t MetadataStore::InsertImage(const ImageRow& row) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const char* sql = R"(
        INSERT INTO images(person_id, role_id, file_path, md5, width, height, format, ingest_status)
        VALUES(?,?,?,?,?,?,?,?)
    )";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        return -1;
    }
    if (row.person_id > 0) sqlite3_bind_int64(stmt, 1, row.person_id); else sqlite3_bind_null(stmt, 1);
    if (row.role_id > 0) sqlite3_bind_int64(stmt, 2, row.role_id); else sqlite3_bind_null(stmt, 2);
    sqlite3_bind_text(stmt, 3, row.file_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, row.md5.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, row.width);
    sqlite3_bind_int(stmt, 6, row.height);
    sqlite3_bind_text(stmt, 7, row.format.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, row.ingest_status.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        lastError_ = sqlite3_errmsg(db_);
        return -1;
    }
    return sqlite3_last_insert_rowid(db_);
}

std::optional<ImageMd5Row> MetadataStore::FindImageByMd5(const std::string& md5) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const char* sql = "SELECT image_id, file_path FROM images WHERE md5=? LIMIT 1";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        return std::nullopt;
    }
    sqlite3_bind_text(stmt, 1, md5.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }
    ImageMd5Row row;
    row.image_id = sqlite3_column_int64(stmt, 0);
    const char* path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    row.file_path = path ? path : "";
    sqlite3_finalize(stmt);
    return row;
}

bool MetadataStore::UpdateImageStatus(int64_t imageId, const std::string& status, const std::string& failReason) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const char* sql = "UPDATE images SET ingest_status=?, fail_reason=? WHERE image_id=?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        return false;
    }
    sqlite3_bind_text(stmt, 1, status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, failReason.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, imageId);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

int64_t MetadataStore::InsertFaceEmbeddingRef(const FaceEmbeddingRef& ref) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const char* sql = R"(
        INSERT INTO face_embeddings(image_id, bbox_x, bbox_y, bbox_w, bbox_h, det_score)
        VALUES(?,?,?,?,?,?)
    )";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        return -1;
    }
    sqlite3_bind_int64(stmt, 1, ref.image_id);
    sqlite3_bind_double(stmt, 2, ref.bbox_x);
    sqlite3_bind_double(stmt, 3, ref.bbox_y);
    sqlite3_bind_double(stmt, 4, ref.bbox_w);
    sqlite3_bind_double(stmt, 5, ref.bbox_h);
    sqlite3_bind_double(stmt, 6, ref.det_score);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        lastError_ = sqlite3_errmsg(db_);
        return -1;
    }
    return sqlite3_last_insert_rowid(db_);
}

std::optional<FaceMatchRow> MetadataStore::ResolveFaceEmbedding(int64_t embeddingId) {
    const char* sql = R"(
        SELECT fe.embedding_id, fe.image_id, i.person_id, p.display_name
        FROM face_embeddings fe
        JOIN images i ON fe.image_id = i.image_id
        LEFT JOIN persons p ON i.person_id = p.person_id
        WHERE fe.embedding_id = ?
    )";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        return std::nullopt;
    }
    sqlite3_bind_int64(stmt, 1, embeddingId);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }
    FaceMatchRow row;
    row.embedding_id = sqlite3_column_int64(stmt, 0);
    row.image_id = sqlite3_column_int64(stmt, 1);
    row.person_id = sqlite3_column_int64(stmt, 2);
    const char* name = (const char*)sqlite3_column_text(stmt, 3);
    row.display_name = name ? name : "";
    sqlite3_finalize(stmt);
    return row;
}

int64_t MetadataStore::InsertClothingEmbeddingRef(const ClothingEmbeddingRef& ref) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const char* sql = R"(
        INSERT INTO clothing_embeddings(image_id, bbox_x, bbox_y, bbox_w, bbox_h, pose_keypoints)
        VALUES(?,?,?,?,?,?)
    )";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        return -1;
    }
    sqlite3_bind_int64(stmt, 1, ref.image_id);
    sqlite3_bind_double(stmt, 2, ref.bbox_x);
    sqlite3_bind_double(stmt, 3, ref.bbox_y);
    sqlite3_bind_double(stmt, 4, ref.bbox_w);
    sqlite3_bind_double(stmt, 5, ref.bbox_h);
    if (ref.pose_keypoints.empty()) {
        sqlite3_bind_null(stmt, 6);
    } else {
        sqlite3_bind_blob(stmt, 6, ref.pose_keypoints.data(),
                           static_cast<int>(ref.pose_keypoints.size() * sizeof(float)), SQLITE_TRANSIENT);
    }
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        lastError_ = sqlite3_errmsg(db_);
        return -1;
    }
    return sqlite3_last_insert_rowid(db_);
}

std::optional<ClothingMatchRow> MetadataStore::ResolveClothingEmbedding(int64_t embeddingId) {
    const char* sql = R"(
        SELECT ce.embedding_id, ce.image_id, i.person_id, p.display_name, i.role_id, r.display_name
        FROM clothing_embeddings ce
        JOIN images i ON ce.image_id = i.image_id
        LEFT JOIN persons p ON i.person_id = p.person_id
        LEFT JOIN roles r ON i.role_id = r.role_id
        WHERE ce.embedding_id = ?
    )";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        return std::nullopt;
    }
    sqlite3_bind_int64(stmt, 1, embeddingId);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }
    ClothingMatchRow row;
    row.embedding_id = sqlite3_column_int64(stmt, 0);
    row.image_id = sqlite3_column_int64(stmt, 1);
    row.person_id = sqlite3_column_int64(stmt, 2);
    const char* name = (const char*)sqlite3_column_text(stmt, 3);
    row.display_name = name ? name : "";
    row.role_id = sqlite3_column_int64(stmt, 4);
    const char* roleName = (const char*)sqlite3_column_text(stmt, 5);
    row.role_name = roleName ? roleName : "";
    sqlite3_finalize(stmt);
    return row;
}

bool MetadataStore::UpdateImagePHash(int64_t imageId, int64_t phash) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const char* sql = "UPDATE images SET phash=? WHERE image_id=?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        return false;
    }
    sqlite3_bind_int64(stmt, 1, phash);
    sqlite3_bind_int64(stmt, 2, imageId);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

std::vector<std::pair<int64_t, int64_t>> MetadataStore::LoadAllPHashes() {
    std::vector<std::pair<int64_t, int64_t>> result;
    const char* sql = "SELECT image_id, phash FROM images WHERE phash IS NOT NULL";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        return result;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int64_t imageId = sqlite3_column_int64(stmt, 0);
        int64_t phash = sqlite3_column_int64(stmt, 1);
        result.emplace_back(imageId, phash);
    }
    sqlite3_finalize(stmt);
    return result;
}

std::optional<std::string> MetadataStore::GetImageFilePath(int64_t imageId) {
    const char* sql = "SELECT file_path FROM images WHERE image_id = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        return std::nullopt;
    }
    sqlite3_bind_int64(stmt, 1, imageId);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }
    const char* path = (const char*)sqlite3_column_text(stmt, 0);
    std::string result = path ? path : "";
    sqlite3_finalize(stmt);
    return result;
}

std::optional<ImagePersonRow> MetadataStore::GetPersonByImageId(int64_t imageId) {
    const char* sql = R"(
        SELECT i.image_id, i.person_id, p.display_name
        FROM images i
        LEFT JOIN persons p ON i.person_id = p.person_id
        WHERE i.image_id = ?
    )";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        return std::nullopt;
    }
    sqlite3_bind_int64(stmt, 1, imageId);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }
    ImagePersonRow row;
    row.image_id = sqlite3_column_int64(stmt, 0);
    row.person_id = sqlite3_column_int64(stmt, 1);
    const char* name = (const char*)sqlite3_column_text(stmt, 2);
    row.display_name = name ? name : "";
    sqlite3_finalize(stmt);
    return row;
}

}  // namespace coser
