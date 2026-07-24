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

        CREATE TABLE IF NOT EXISTS images (
            image_id      INTEGER PRIMARY KEY AUTOINCREMENT,
            person_id     INTEGER REFERENCES persons(person_id),
            file_path     TEXT NOT NULL UNIQUE,
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
    )";
    return Execute(sql);
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

int64_t MetadataStore::InsertImage(const ImageRow& row) {
    const char* sql = R"(
        INSERT INTO images(person_id, file_path, width, height, format, ingest_status)
        VALUES(?,?,?,?,?,?)
    )";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        return -1;
    }
    sqlite3_bind_int64(stmt, 1, row.person_id);
    sqlite3_bind_text(stmt, 2, row.file_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, row.width);
    sqlite3_bind_int(stmt, 4, row.height);
    sqlite3_bind_text(stmt, 5, row.format.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, row.ingest_status.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        lastError_ = sqlite3_errmsg(db_);
        return -1;
    }
    return sqlite3_last_insert_rowid(db_);
}

bool MetadataStore::UpdateImageStatus(int64_t imageId, const std::string& status, const std::string& failReason) {
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

}  // namespace coser
