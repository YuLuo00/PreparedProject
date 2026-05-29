#include <string>
#include <vector>
#include <algorithm>

class ZYB_ARCHIVE_TOOL_API PwdManager
{
public:
    PwdManager()
    {
        this->Init();
    };
    static PwdManager &Ins()
    {
        static PwdManager ins;
        return ins;
    }

    void Init();

    bool AddNewPwd(const std::string &pwd);
    std::vector<std::string> GetAllPwd();

    // 将指定密码移到最前面（匹配成功后调用，提升下次命中速度）
    bool PromotePwd(const std::string &pwd);

private:
    void SaveToFile();

    std::string m_pwdBoolPath = "./pwd.dat";
    std::vector<std::string> m_pwdBook;  // 改为 vector，保持顺序
};
