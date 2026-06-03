#include "Database.h"
#include <sstream>
#include <filesystem>
#include <cstring>

namespace fs = std::filesystem;

Database::Database() = default;

Database::~Database() {
    Close();
}

bool Database::Open(const std::string& dbPath) {
    // 确保目录存在
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
    // 开启 WAL 模式提升并发性能
    Execute("PRAGMA journal_mode=WAL;");
    Execute("PRAGMA synchronous=NORMAL;");
    Execute("PRAGMA foreign_keys=ON;");
    return CreateTables();
}

void Database::Close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::Execute(const std::string& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        lastError_ = errMsg ? errMsg : "unknown error";
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

bool Database::Prepare(const std::string& sql, sqlite3_stmt** stmt) {
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, stmt, nullptr);
    if (rc != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        return false;
    }
    return true;
}

bool Database::CreateTables() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS images (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            path        TEXT    NOT NULL UNIQUE,
            filename    TEXT    NOT NULL,
            shoot_date  TEXT    DEFAULT '',
            rating      REAL    DEFAULT 0.0,
            path_prefix TEXT    DEFAULT '',
            scene_tags  TEXT    DEFAULT '[]',
            style_tags  TEXT    DEFAULT '[]',
            mood_tags   TEXT    DEFAULT '[]',
            created_at  TEXT    DEFAULT (datetime('now'))
        );

        CREATE TABLE IF NOT EXISTS image_features (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            image_id        INTEGER NOT NULL UNIQUE,
            clip_vector     BLOB,
            wd14_vector     BLOB,
            clip_faiss_id   INTEGER DEFAULT -1,
            wd14_faiss_id   INTEGER DEFAULT -1,
            FOREIGN KEY(image_id) REFERENCES images(id)
        );

        CREATE TABLE IF NOT EXISTS image_tags (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            image_id    INTEGER NOT NULL,
            tag         TEXT    NOT NULL,
            confidence  REAL    DEFAULT 1.0,
            FOREIGN KEY(image_id) REFERENCES images(id)
        );
        CREATE INDEX IF NOT EXISTS idx_image_tags_image_id ON image_tags(image_id);
        CREATE INDEX IF NOT EXISTS idx_image_tags_tag ON image_tags(tag);

        CREATE TABLE IF NOT EXISTS coser_faces (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            coser_name      TEXT    NOT NULL,
            reference_image TEXT    NOT NULL,
            face_vector     BLOB    NOT NULL,
            face_faiss_id   INTEGER DEFAULT -1,
            created_at      TEXT    DEFAULT (datetime('now'))
        );
        CREATE INDEX IF NOT EXISTS idx_coser_faces_name ON coser_faces(coser_name);

        CREATE TABLE IF NOT EXISTS faiss_id_map (
            faiss_id    INTEGER NOT NULL,
            entity_id   INTEGER NOT NULL,
            index_type  TEXT    NOT NULL,
            PRIMARY KEY (faiss_id, index_type)
        );
        CREATE INDEX IF NOT EXISTS idx_faiss_map_type ON faiss_id_map(index_type);

        CREATE TABLE IF NOT EXISTS faiss_id_counter (
            index_type  TEXT    PRIMARY KEY,
            next_id     INTEGER DEFAULT 0
        );
        INSERT OR IGNORE INTO faiss_id_counter(index_type, next_id) VALUES('clip', 0);
        INSERT OR IGNORE INTO faiss_id_counter(index_type, next_id) VALUES('wd14', 0);
        INSERT OR IGNORE INTO faiss_id_counter(index_type, next_id) VALUES('face', 0);
    )";
    return Execute(sql);
}

int64_t Database::InsertImage(const ImageRecord& rec) {
    const char* sql = R"(
        INSERT OR IGNORE INTO images(path, filename, shoot_date, rating, path_prefix,
                                     scene_tags, style_tags, mood_tags)
        VALUES(?,?,?,?,?,?,?,?)
    )";
    sqlite3_stmt* stmt = nullptr;
    if (!Prepare(sql, &stmt)) return -1;

    sqlite3_bind_text(stmt, 1, rec.path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, rec.filename.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, rec.shoot_date.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 4, rec.rating);
    sqlite3_bind_text(stmt, 5, rec.path_prefix.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, rec.scene_tags.empty() ? "[]" : rec.scene_tags.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, rec.style_tags.empty() ? "[]" : rec.style_tags.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, rec.mood_tags.empty() ? "[]" : rec.mood_tags.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        lastError_ = sqlite3_errmsg(db_);
        return -1;
    }
    int64_t rowId = sqlite3_last_insert_rowid(db_);
    if (rowId == 0) {
        // 已存在，查询 id
        const char* selSql = "SELECT id FROM images WHERE path=?";
        if (!Prepare(selSql, &stmt)) return -1;
        sqlite3_bind_text(stmt, 1, rec.path.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            rowId = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return rowId;
}

bool Database::UpdateImageFeatures(int64_t imageId,
                                    const std::vector<float>& clipVec,
                                    const std::vector<float>& wd14Vec,
                                    int64_t clipFaissId,
                                    int64_t wd14FaissId) {
    const char* sql = R"(
        INSERT INTO image_features(image_id, clip_vector, wd14_vector, clip_faiss_id, wd14_faiss_id)
        VALUES(?,?,?,?,?)
        ON CONFLICT(image_id) DO UPDATE SET
            clip_vector=excluded.clip_vector,
            wd14_vector=excluded.wd14_vector,
            clip_faiss_id=excluded.clip_faiss_id,
            wd14_faiss_id=excluded.wd14_faiss_id
    )";
    sqlite3_stmt* stmt = nullptr;
    if (!Prepare(sql, &stmt)) return false;

    sqlite3_bind_int64(stmt, 1, imageId);
    if (!clipVec.empty())
        sqlite3_bind_blob(stmt, 2, clipVec.data(), (int)(clipVec.size() * sizeof(float)), SQLITE_TRANSIENT);
    else
        sqlite3_bind_null(stmt, 2);
    if (!wd14Vec.empty())
        sqlite3_bind_blob(stmt, 3, wd14Vec.data(), (int)(wd14Vec.size() * sizeof(float)), SQLITE_TRANSIENT);
    else
        sqlite3_bind_null(stmt, 3);
    sqlite3_bind_int64(stmt, 4, clipFaissId);
    sqlite3_bind_int64(stmt, 5, wd14FaissId);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool Database::ImageExists(const std::string& path) {
    const char* sql = "SELECT 1 FROM images WHERE path=? LIMIT 1";
    sqlite3_stmt* stmt = nullptr;
    if (!Prepare(sql, &stmt)) return false;
    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_TRANSIENT);
    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return exists;
}

std::vector<ImageRecord> Database::QueryImages(const QueryFilter& filter) {
    std::vector<ImageRecord> results;
    std::ostringstream ss;
    ss << "SELECT id, path, filename, shoot_date, rating, path_prefix, scene_tags, style_tags, mood_tags FROM images WHERE 1=1";

    if (!filter.date_from.empty())
        ss << " AND shoot_date >= '" << filter.date_from << "'";
    if (!filter.date_to.empty())
        ss << " AND shoot_date <= '" << filter.date_to << "'";
    if (!filter.path_prefix.empty())
        ss << " AND path_prefix LIKE '" << filter.path_prefix << "%'";
    if (filter.rating_min > 0.0)
        ss << " AND rating >= " << filter.rating_min;

    // 标签过滤：通过 image_tags 表 JOIN
    if (!filter.tags.empty()) {
        ss << " AND id IN (SELECT DISTINCT image_id FROM image_tags WHERE tag IN (";
        for (size_t i = 0; i < filter.tags.size(); ++i) {
            if (i > 0) ss << ",";
            ss << "'" << filter.tags[i] << "'";
        }
        ss << "))";
    }

    ss << " ORDER BY rating DESC, shoot_date DESC LIMIT " << filter.top_k;

    sqlite3_stmt* stmt = nullptr;
    if (!Prepare(ss.str(), &stmt)) return results;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ImageRecord rec;
        rec.id = sqlite3_column_int64(stmt, 0);
        auto getText = [&](int col) -> std::string {
            const char* t = (const char*)sqlite3_column_text(stmt, col);
            return t ? t : "";
        };
        rec.path = getText(1);
        rec.filename = getText(2);
        rec.shoot_date = getText(3);
        rec.rating = sqlite3_column_double(stmt, 4);
        rec.path_prefix = getText(5);
        rec.scene_tags = getText(6);
        rec.style_tags = getText(7);
        rec.mood_tags = getText(8);
        results.push_back(rec);
    }
    sqlite3_finalize(stmt);
    return results;
}

std::optional<ImageRecord> Database::GetImageById(int64_t id) {
    const char* sql = "SELECT id, path, filename, shoot_date, rating, path_prefix, scene_tags, style_tags, mood_tags FROM images WHERE id=?";
    sqlite3_stmt* stmt = nullptr;
    if (!Prepare(sql, &stmt)) return std::nullopt;
    sqlite3_bind_int64(stmt, 1, id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }
    ImageRecord rec;
    rec.id = sqlite3_column_int64(stmt, 0);
    auto getText = [&](int col) -> std::string {
        const char* t = (const char*)sqlite3_column_text(stmt, col);
        return t ? t : "";
    };
    rec.path = getText(1);
    rec.filename = getText(2);
    rec.shoot_date = getText(3);
    rec.rating = sqlite3_column_double(stmt, 4);
    rec.path_prefix = getText(5);
    rec.scene_tags = getText(6);
    rec.style_tags = getText(7);
    rec.mood_tags = getText(8);
    sqlite3_finalize(stmt);
    return rec;
}

bool Database::InsertTags(int64_t imageId, const std::vector<std::pair<std::string,float>>& tags) {
    // 先删除旧标签
    {
        const char* delSql = "DELETE FROM image_tags WHERE image_id=?";
        sqlite3_stmt* stmt = nullptr;
        if (!Prepare(delSql, &stmt)) return false;
        sqlite3_bind_int64(stmt, 1, imageId);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    const char* sql = "INSERT INTO image_tags(image_id, tag, confidence) VALUES(?,?,?)";
    sqlite3_stmt* stmt = nullptr;
    if (!Prepare(sql, &stmt)) return false;
    for (auto& [tag, conf] : tags) {
        sqlite3_reset(stmt);
        sqlite3_bind_int64(stmt, 1, imageId);
        sqlite3_bind_text(stmt, 2, tag.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 3, conf);
        sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);
    return true;
}

std::vector<std::pair<std::string,float>> Database::GetTags(int64_t imageId) {
    std::vector<std::pair<std::string,float>> tags;
    const char* sql = "SELECT tag, confidence FROM image_tags WHERE image_id=? ORDER BY confidence DESC";
    sqlite3_stmt* stmt = nullptr;
    if (!Prepare(sql, &stmt)) return tags;
    sqlite3_bind_int64(stmt, 1, imageId);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* t = (const char*)sqlite3_column_text(stmt, 0);
        float c = (float)sqlite3_column_double(stmt, 1);
        if (t) tags.emplace_back(t, c);
    }
    sqlite3_finalize(stmt);
    return tags;
}

int64_t Database::GetNextFaissId(const std::string& indexType) {
    const char* sql = R"(
        UPDATE faiss_id_counter SET next_id = next_id + 1 WHERE index_type=?;
        SELECT next_id - 1 FROM faiss_id_counter WHERE index_type=?;
    )";
    // 分两步执行
    {
        const char* upd = "UPDATE faiss_id_counter SET next_id = next_id + 1 WHERE index_type=?";
        sqlite3_stmt* stmt = nullptr;
        if (!Prepare(upd, &stmt)) return -1;
        sqlite3_bind_text(stmt, 1, indexType.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    {
        const char* sel = "SELECT next_id - 1 FROM faiss_id_counter WHERE index_type=?";
        sqlite3_stmt* stmt = nullptr;
        if (!Prepare(sel, &stmt)) return -1;
        sqlite3_bind_text(stmt, 1, indexType.c_str(), -1, SQLITE_TRANSIENT);
        int64_t id = -1;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            id = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
        return id;
    }
}

bool Database::InsertFaissMapping(int64_t faissId, int64_t entityId, const std::string& indexType) {
    const char* sql = R"(
        INSERT OR REPLACE INTO faiss_id_map(faiss_id, entity_id, index_type) VALUES(?,?,?)
    )";
    sqlite3_stmt* stmt = nullptr;
    if (!Prepare(sql, &stmt)) return false;
    sqlite3_bind_int64(stmt, 1, faissId);
    sqlite3_bind_int64(stmt, 2, entityId);
    sqlite3_bind_text(stmt, 3, indexType.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

int64_t Database::GetEntityIdByFaissId(int64_t faissId, const std::string& indexType) {
    const char* sql = "SELECT entity_id FROM faiss_id_map WHERE faiss_id=? AND index_type=?";
    sqlite3_stmt* stmt = nullptr;
    if (!Prepare(sql, &stmt)) return -1;
    sqlite3_bind_int64(stmt, 1, faissId);
    sqlite3_bind_text(stmt, 2, indexType.c_str(), -1, SQLITE_TRANSIENT);
    int64_t id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        id = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return id;
}

bool Database::LoadAllVectors(const std::string& indexType,
                               std::vector<int64_t>& outFaissIds,
                               std::vector<std::vector<float>>& outVectors) {
    outFaissIds.clear();
    outVectors.clear();

    std::string sql;
    if (indexType == "clip") {
        sql = R"(
            SELECT fm.faiss_id, f.clip_vector
            FROM faiss_id_map fm
            JOIN image_features f ON fm.entity_id = f.image_id
            WHERE fm.index_type='clip' AND f.clip_vector IS NOT NULL
            ORDER BY fm.faiss_id
        )";
    } else if (indexType == "wd14") {
        sql = R"(
            SELECT fm.faiss_id, f.wd14_vector
            FROM faiss_id_map fm
            JOIN image_features f ON fm.entity_id = f.image_id
            WHERE fm.index_type='wd14' AND f.wd14_vector IS NOT NULL
            ORDER BY fm.faiss_id
        )";
    } else if (indexType == "face") {
        sql = R"(
            SELECT fm.faiss_id, cf.face_vector
            FROM faiss_id_map fm
            JOIN coser_faces cf ON fm.entity_id = cf.id
            WHERE fm.index_type='face' AND cf.face_vector IS NOT NULL
            ORDER BY fm.faiss_id
        )";
    } else {
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    if (!Prepare(sql, &stmt)) return false;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int64_t fid = sqlite3_column_int64(stmt, 0);
        const void* blob = sqlite3_column_blob(stmt, 1);
        int blobSize = sqlite3_column_bytes(stmt, 1);
        if (blob && blobSize > 0) {
            int numFloats = blobSize / sizeof(float);
            std::vector<float> vec(numFloats);
            memcpy(vec.data(), blob, blobSize);
            outFaissIds.push_back(fid);
            outVectors.push_back(std::move(vec));
        }
    }
    sqlite3_finalize(stmt);
    return true;
}

int64_t Database::InsertCoserFace(const CoserFace& face) {
    const char* sql = R"(
        INSERT INTO coser_faces(coser_name, reference_image, face_vector)
        VALUES(?,?,?)
    )";
    sqlite3_stmt* stmt = nullptr;
    if (!Prepare(sql, &stmt)) return -1;
    sqlite3_bind_text(stmt, 1, face.coser_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, face.reference_image.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 3, face.face_vector.data(),
                      (int)(face.face_vector.size() * sizeof(float)), SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return -1;
    return sqlite3_last_insert_rowid(db_);
}

std::vector<CoserFace> Database::GetAllCoserFaces() {
    std::vector<CoserFace> faces;
    const char* sql = "SELECT id, coser_name, reference_image, face_vector FROM coser_faces";
    sqlite3_stmt* stmt = nullptr;
    if (!Prepare(sql, &stmt)) return faces;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        CoserFace f;
        f.id = sqlite3_column_int64(stmt, 0);
        const char* name = (const char*)sqlite3_column_text(stmt, 1);
        const char* ref = (const char*)sqlite3_column_text(stmt, 2);
        f.coser_name = name ? name : "";
        f.reference_image = ref ? ref : "";
        const void* blob = sqlite3_column_blob(stmt, 3);
        int blobSize = sqlite3_column_bytes(stmt, 3);
        if (blob && blobSize > 0) {
            int numFloats = blobSize / sizeof(float);
            f.face_vector.resize(numFloats);
            memcpy(f.face_vector.data(), blob, blobSize);
        }
        faces.push_back(std::move(f));
    }
    sqlite3_finalize(stmt);
    return faces;
}

bool Database::CoserExists(const std::string& name) {
    const char* sql = "SELECT 1 FROM coser_faces WHERE coser_name=? LIMIT 1";
    sqlite3_stmt* stmt = nullptr;
    if (!Prepare(sql, &stmt)) return false;
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return exists;
}

bool Database::BeginTransaction() { return Execute("BEGIN TRANSACTION"); }
bool Database::CommitTransaction() { return Execute("COMMIT"); }
bool Database::RollbackTransaction() { return Execute("ROLLBACK"); }
