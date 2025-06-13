#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <string>
namespace fs = std::filesystem;

#include <fcntl.h>
#include <io.h>

#include "CommonTool.h"
#include "WinTool.h"

class RawRemoveCore
{
public:
    RawRemoveCore() = default;

    std::wstring m_rootDir;
    std::wstring m_rawStoreDir;
    using FilesSetsType = std::map<std::wstring, std::set<std::wstring, CaseInsCmpW>, CaseInsCmpW>;
    FilesSetsType m_raw2Fullpath;
    FilesSetsType m_pic2Fullpath;
    enum class FileType
    {
        PIC,
        ARW,
    };
    std::wstring GetFileLoaction(const std::wstring fileName, FileType type) const
    {
        const std::wstring fileStemName = fs::path(fileName).stem().wstring();
        const FilesSetsType *fileSet = nullptr;
        switch (type) {
            case RawRemoveCore::FileType::PIC: {
                fileSet = &this->m_pic2Fullpath;
                break;
            }
            case RawRemoveCore::FileType::ARW: {
                fileSet = &this->m_raw2Fullpath;
                break;
            }
            default:
                throw std::exception("file type error");
                break;
        }
        if (fileSet->count(fileName) == 0) {
            return L"";
        }
        const std::set<std::wstring, CaseInsCmpW> &files = fileSet->at(fileName);
        if (files.size() != 1) {
            throw std::exception("找到了多个文件");
            return L"";
        }
        const std::wstring &fileFolder = *files.begin();
        static const std::map<FileType, std::wstring> type2Ext{{FileType::ARW, L"arw"}, {FileType::PIC, L"jpg"}};
        //std::wstring fileExt = 
        std::wstring fileFullPath = (fs::path(fileFolder) / fileName).generic_wstring();
        return fileFullPath;
    }

    void ParseFileLocation()
    {
        bool isRawStoreDir = false;
        FindFilesDfs(
            m_rootDir,
            [&](const std::wstring &folderPath, const WIN32_FIND_DATAW &data) {
                const std::wstring fileFullPath = (fs::path(folderPath) / data.cFileName).generic_wstring();
                if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    //std::wcout << L"这是一个目录" << std::endl;
                    //std::wcout << L"这是一个目录" << data.cFileName << std::endl;
                }
                else {
                    fs::path path = data.cFileName;
                    std::wstring fileName = data.cFileName;
                    std::wstring extName = path.extension().wstring();
                    std::wcout << L"这是一个普通文件" << data.cFileName << std::endl;
                    std::set<std::wstring, CaseInsCmpW> picExts{
                        L".jpg",
                        L".png",
                        L".jpeg",
                    };
                    if (picExts.count(extName) > 0) {
                        m_pic2Fullpath[fileName].insert(fileFullPath);
                    }
                    else if (CaseInsCmpW::sameIns(extName, L".arw")) {
                        m_raw2Fullpath[fileName].insert(fileFullPath);
                    }
                }
                return GoOnFind::CONTINUE;
            },
            [&](bool isInto, const std::wstring &folderName) {
                if (fs::canonical(folderName) == fs::canonical(m_rawStoreDir)) {
                    isRawStoreDir = isInto;
                }
            });
        return;
    }

    void RemoveUnuseRawsTo(const std::wstring &folderPath = L"")
    {
        // 检查多余的raw
        std::set<std::wstring, CaseInsCmpW> notUseStem;
        std::set<std::wstring, CaseInsCmpW> picsStem;
        for (const auto &[pic, _] : m_pic2Fullpath) {
            std::wstring picStem = fs::path(pic).stem().wstring();
            picsStem.insert(picStem);
        }
        std::set<std::wstring, CaseInsCmpW> rawsStem;
        for (const auto &[raw, _] : m_raw2Fullpath) {
            std::wstring rawStem = fs::path(raw).stem().wstring();
            rawsStem.insert(rawStem);
        }

        std::set_difference(
            rawsStem.begin(), rawsStem.end(), picsStem.begin(), picsStem.end(), std::inserter(notUseStem, notUseStem.end()), CaseInsCmpW{});
        // 删掉/移动
        int count = notUseStem.size();
        for (const std::wstring &rawStemNotUse : notUseStem) {
            const std::set<std::wstring, CaseInsCmpW> &rawFolders = m_raw2Fullpath[rawStemNotUse + L".arw"];
            for (const std::wstring &rawFoler : rawFolders) {
                std::wstring rawPath = rawFoler + L"\\" + rawStemNotUse + L".arw";
                if (folderPath.empty()) {
                    throw std::exception("禁止删除功能, 请指定raw的移动目标路径替代回收站");
                    //fs::remove(rawPath);
                    WinTools::moveToRecycleBin(rawPath);
                    std::wcout << "remove " << rawPath << std::endl;
                }
                else {
                    std::wstring toPath = folderPath + L"\\" + rawStemNotUse;
                    fs::rename(rawPath, toPath);
                    std::wcout << "move " << rawPath << " to " << toPath << std::endl;
                }
            }
        }

        return;
    }

    void MoveAllRawsToStoreDir()
    {

    }

    void MoveRaw2Pic(const std::wstring &fileName)
    {
        fs::path fp(fileName);
        
    }
};

void main()
{
    //// 设置控制台为 UTF-8 编码
    //SetConsoleOutputCP(CP_UTF8);
    // 设置控制台为 UTF-16 编码
    int retSetMode = _setmode(_fileno(stdout), _O_U16TEXT);
    std::wcout << L"Hello 控制台" << std::endl;

    RawRemoveCore core;

    core.m_rootDir = LR"(D:\_File\a6700\2025-05-31-深圳环形动漫嘉年华)";
    core.m_rawStoreDir = LR"(D:\_File\a6700\2025-05-31-深圳环形动漫嘉年华\ARW)";

    core.ParseFileLocation();
    //core.RemoveUnuseRawsTo();
    //core.MoveRaw2Pic()

    return;
}