#ifndef MSG2UI_H
#define MSG2UI_H

#include <functional>
#include <set>
#include <string>
#include <vector>

typedef void (*MsgCBFuncPtr)(const wchar_t *, void *, int level);
class ZYB_RAWMOVE_API Msg2UI
{
public:
    Msg2UI() = default;
    ~Msg2UI() = default;
    Msg2UI &GlobalInst()
    {
        static Msg2UI inst;
        return inst;
    }

    // call back for external module
    void SetCallBackWrnMsg(MsgCBFuncPtr cb)
    {
        this->m_CallBackWrnMsg = cb;
    }
    void SetCallBackErrMsg(MsgCBFuncPtr cb)
    {
        this->m_CallBackErrMsg = cb;

        std::wstring msgTest = L"test测试文字";
        this->m_CallBackErrMsg(msgTest.c_str(), nullptr, 0);
    }
    void AddWrnObj(void *obj)
    {
        m_WrnObjs.insert(obj);
    }
    void AddErrObj(void *obj)
    {
        m_ErrObjs.insert(obj);
    }
    void RemoveWrnObj(void *obj)
    {
        m_WrnObjs.erase(obj);
    }
    void RemoveErrObj(void *obj)
    {
        m_ErrObjs.erase(obj);
    }
    void test()
    {
        std::wstring testMsg = L"tes测试字符";
        if (m_CallBackErrMsg) {
            m_CallBackErrMsg(testMsg.c_str(), nullptr, 0);
            for (void *errObj : this->m_ErrObjs) {
                m_CallBackErrMsg(testMsg.c_str(), errObj, 0);
            }
        }
        if (m_CallBackWrnMsg) {
            m_CallBackWrnMsg(testMsg.c_str(), nullptr, 1);
            for (void *wrnObj : this->m_WrnObjs) {
                m_CallBackWrnMsg(testMsg.c_str(), wrnObj, 1);
            }
        }
    }

private:
    MsgCBFuncPtr m_CallBackWrnMsg = nullptr;
    std::set<void *> m_WrnObjs;
    MsgCBFuncPtr m_CallBackErrMsg = nullptr;
    std::set<void *> m_ErrObjs;
};

#endif