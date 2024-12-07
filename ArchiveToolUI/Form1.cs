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
    /// �������ļ�·��
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
    /// ��̨�������뱾
    ///     ��������
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
        // �����첽����
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
                    this.utb_msg.Text += "�ɹ�" + Environment.NewLine;
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
            throw new ArgumentNullException(nameof(callable), "�ɵ��ö�����Ϊ��");

        if(this.InvokeRequired) { // �����Ҫ���̵߳���
            this.Invoke(callable); // �� UI �߳��ϵ���
        }
        else {
            callable(); // ֱ���ڵ�ǰ�̵߳���
        }
    }

    static void CallDelegate(Delegate callable, params object[] args)
    {
        if(callable == null) {
            throw new ArgumentNullException(nameof(callable), "�ɵ��ö�����Ϊ��");
        }

        callable.DynamicInvoke(args); // ��̬����
    }
}
