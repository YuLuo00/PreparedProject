#include "BiliCache.h"

#include <nlohmann/json.hpp>
using js = nlohmann::json;

#include "tools.h"


// compile: g++ -std=c++17 collect_structured_paths.cpp -o collect_structured_paths


// 工具：检查 dir/filename 是否存在且为常规文件
static bool file_exists(const fs::path &dir, const std::string &filename)
{
    try {
        fs::path p = dir / filename;
        return fs::exists(p) && fs::is_regular_file(p);
    }
    catch (const fs::filesystem_error &) {
        return false;
    }
}

// 检查并返回子目录中三个媒体文件的路径（若都存在则返回 true 并填充 out）
bool collect_media_files(const fs::path &child, MediaSubdir &out)
{
    try {
        fs::path p_video = child / "video.m4s";
        fs::path p_audio = child / "audio.m4s";
        fs::path p_index = child / "index.json";
        if (fs::exists(p_video) && fs::is_regular_file(p_video) && fs::exists(p_audio) &&
            fs::is_regular_file(p_audio) && fs::exists(p_index) && fs::is_regular_file(p_index)) {
            out.dir = child;
            out.video = p_video;
            out.audio = p_audio;
            out.index = p_index;
            return true;
        }
    }
    catch (const fs::filesystem_error &) {
        // 忽略无法访问的子项
    }
    return false;
}

// 从 root 开始递归扫描，返回所有符合条件的 Match 结构体
std::vector<Match> BiliCache::CollectBiliFoldersStructured(const fs::path &root)
{
    std::vector<Match> results;
    if (!fs::exists(root) || !fs::is_directory(root))
        return results;

    try {
        for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied), end; it != end;
             ++it) {
            try {
                if (!it->is_directory())
                    continue;
                fs::path dir = it->path();

                // 必须同时存在 entry.json 和 danmaku.xml（在 dir 下）
                fs::path p_entry = dir / "entry.json";
                fs::path p_danmaku = dir / "danmaku.xml";
                if (!(fs::exists(p_entry) && fs::is_regular_file(p_entry)))
                    continue;
                if (!(fs::exists(p_danmaku) && fs::is_regular_file(p_danmaku)))
                    continue;

                Match m;
                m.dir = dir;
                m.entry_json_path = p_entry;
                m.danmaku_xml_path = p_danmaku;

                // 遍历 dir 的直接子目录，收集满足 media 条件的子目录
                try {
                    for (auto const &entry : fs::directory_iterator(dir)) {
                        if (!entry.is_directory())
                            continue;
                        MediaSubdir ms;
                        ms.match = &m;
                        if (collect_media_files(entry.path(), ms)) {
                            m.media_subdirs.push_back(std::move(ms));
                        }
                    }
                    results.push_back(m);
                }
                catch (const fs::filesystem_error &) {
                    // 忽略无法访问的子项
                }

                //if (!m.media_subdirs.empty()) {
                //    results.push_back(std::move(m));
                //    // 跳过该目录的子树，避免重复发现更深层的匹配
                //    it.disable_recursion_pending();
                //}
            }
            catch (const fs::filesystem_error &) {
                // 忽略单个条目错误，继续扫描
            }
        }
    }
    catch (const fs::filesystem_error &) {
        // 根目录不可读或其他全局错误，直接返回已收集的结果
    }

    return results;
}

// 示例：打印结果（每个文件单独字段显示完整路径）
int main2()
{

    fs::path root = R"(C:\Users\Administrator\Desktop\bili_zip_1)";
    auto matches = BiliCache::CollectBiliFoldersStructured(root);
    for (const auto &m : matches) {
        //std::cout << "Parent dir: " << m.dir.string() << "\n";
        //std::cout << "  entry.json: " << m.entry_json_path.string() << "\n";
        //std::cout << "  danmaku.xml: " << m.danmaku_xml_path.string() << "\n";
        //std::cout << "  media subdirs:\n";
        //for (const auto &c : m.media_subdirs) {
        //    std::cout << "    - subdir: " << c.dir.string() << "\n";
        //    std::cout << "      video: " << c.video.string() << "\n";
        //    std::cout << "      audio: " << c.audio.string() << "\n";
        //    std::cout << "      index: " << c.index.string() << "\n";
        //}
        std::string title = BiliCache::GetTitle(m);
        std::string title_local = Tools::Utf8ToLocal(title);
        std::cout << title_local << std::endl;
        // 剪切到其他路径
        {
            //auto dirName = m.dir.filename();
            //auto relativePath = m.dir.lexically_relative(root);
            //fs::path aimDir = fs::path(R"(C:\Users\Administrator\Desktop\bili_zip_1\type1)") / relativePath;
            //if (fs::exists(aimDir.parent_path()) == false) {
            //    fs::create_directories(aimDir.parent_path());
            //    if (fs::exists(aimDir) == false) {
            //        continue;
            //    }
            //}
            //std::error_code ec;

            //fs::rename(m.dir, aimDir, ec);
            ////fs::rename(src, dst, ec); // 不抛异常，错误信息写入 ec
            //if (ec) {
            //    std::cerr << m.dir << "\n\t>>>" << aimDir << "\n"
            //              << " --- rename failed: " << ec.value() << " -> " << ec.message() << "\n";
            //    continue;
            //}
            //else {
            //    continue;
            //}
        }
        //continue;
    }
    std::cout << "Found " << matches.size() << " matching folders.\n";
    return 0;
}

std::string BiliCache::GetTitle(const std::wstring &entryPath)
{
    json js = Tools::LoadJsonFromFile(entryPath);
    std::string title = js.at("title").get<std::string>();
    std::string buffer = js.dump(2);
    return title;
}

std::string BiliCache::GetTitle(const Match &match)
{
    return GetTitle(match.entry_json_path.generic_wstring());
}