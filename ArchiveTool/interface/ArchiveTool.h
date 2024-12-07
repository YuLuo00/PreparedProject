#include <string>
#include <vector>
ZYB_ARCHIVE_TOOL_API bool ArchiveExtraTest(const std::string &file, const std::string &passwd, const std::string &type);

ZYB_ARCHIVE_TOOL_API std::string check_format(const std::string &filePath);

ZYB_ARCHIVE_TOOL_API std::vector<std::string> GetKeys();
ZYB_ARCHIVE_TOOL_API void UpdateTable(const std::string &key, const std::string &type);