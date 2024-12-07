#include <string>
#include <vector>
#include <set>

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

private:
    std::string m_pwdBoolPath = "D:/_EnviromentConfiguration/_my_path/pwd.dat";
    std::set<std::string> m_pwdBook;

};