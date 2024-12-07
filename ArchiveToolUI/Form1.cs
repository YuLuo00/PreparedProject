namespace ArchiveToolUI;

//using ClassLibrary2;
using NS_ArchiveToolCLR;

using System.Text;

public partial class MainUI : Form
{
    public MainUI()
    {
        InitializeComponent();
    }

    /// <summary>
    /// 待处理文件路径
    /// </summary>
    public string? FilePath { get; set; } = null;

    private void ub_addNewPwd_Click(object sender, EventArgs e)
    {
        string newPwd = this.utb_newPwd.Text;
        if(string.IsNullOrEmpty(newPwd)) {
            return;
        }
        //PwdManagerCLR.AddPws(newPwd);
    }

    private void MainUI_Load(object sender, EventArgs e)
    {
        this.toolStripStatusLabel1.Text = "";
    }

    private bool m_isCheckingPwd = false;

    /// <summary>
    /// 后台遍历密码本
    ///     不可重入
    /// </summary>
    /// <param name="sender"></param>
    /// <param name="e"></param>
    private void ub_tryGetPwd_Click(object sender, EventArgs e)
    {
        //int ii = Class1.test();
        string format = ArchiveToolCLR.CheckFormatCLR(this.FilePath);
        this.utb_archiveHeader.Text = format;
        if(this.m_isCheckingPwd) {
            return;
        }
        this.m_isCheckingPwd = true;
        this.ub_tryGetPwd.Enabled = false;
        // 启动异步任务
        Task task = Task.Run(async () =>
        {
            string format = ArchiveToolCLR.CheckFormatCLR(this.FilePath);
            List<String> pwds = PwdManagerCLR.GetAllPasswords();
            int size = pwds.Count;
            for(int i = 0; i < size; i++) {
                String pwd = pwds[i];
                this.InvokeCall(() =>
                {
                    this.utb_curPwd.Text = pwd;
                    this.utb_msg.Text += i.ToString() + "/" + size.ToString() + Environment.NewLine;
                });
                if(string.IsNullOrEmpty(pwd)) {
                    continue;
                }
                if(!ArchiveToolCLR.ArchiveExtraTestCLR(this.FilePath, pwd)) {
                    continue;
                }
                this.InvokeCall(() =>
                {
                    this.utb_msg.Text += "成功" + Environment.NewLine;
                });
                break;
            }

            this.InvokeCall(() =>
            {
                this.m_isCheckingPwd = false;
                this.ub_tryGetPwd.Enabled = false;
            });
        });
    }

    private void InvokeCall(Action callable)
    {
        if(callable == null)
            throw new ArgumentNullException(nameof(callable), "可调用对象不能为空");

        if(this.InvokeRequired) { // 如果需要跨线程调用
            this.Invoke(callable); // 在 UI 线程上调用
        }
        else {
            callable(); // 直接在当前线程调用
        }
    }

    static void CallDelegate(Delegate callable, params object[] args)
    {
        if(callable == null) {
            throw new ArgumentNullException(nameof(callable), "可调用对象不能为空");
        }

        callable.DynamicInvoke(args); // 动态调用
    }
}
