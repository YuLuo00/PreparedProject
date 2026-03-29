#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
namespace fs = std::filesystem;

// 单个媒体子目录信息：目录 + 三个文件的完整路径
struct MediaSubdir
{
    fs::path dir;   // 子目录路径，例如 ...\1\80
    fs::path video; // 子目录/video.m4s
    fs::path audio; // 子目录/audio.m4s
    fs::path index; // 子目录/index.json
};

// 匹配结果：父目录 + 单独字段存路径 + 所有匹配的媒体子目录
struct Match
{
    fs::path dir;                           // 符合条件的父目录，例如 ...\10723293\1
    fs::path entry_json_path;               // dir/entry.json 的完整路径（若存在）
    fs::path danmaku_xml_path;              // dir/danmaku.xml 的完整路径（若存在）
    std::vector<MediaSubdir> media_subdirs; // 所有符合条件的子目录
};

std::vector<Match> CollectBiliFoldersStructured(const fs::path &root);

int main2();