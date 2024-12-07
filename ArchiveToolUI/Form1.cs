namespace ArchiveToolUI;
using System;
using System.Runtime.InteropServices;

//using ClassLibrary2;
using NS_ArchiveToolCLR;

using System.Text;
using System.Runtime.CompilerServices;
using System.Reflection;
using System.Windows.Forms;

public partial class MainUI : Form
{
    public MainUI()
    {
        InitializeComponent();
        this.AllowDrop = true;
        this.DoubleBuffered = true;
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
        ArchiveToolCLR.GetKeysCLR().ForEach(x =>
        {
            this.ucb_archiveType.Items.Add(x);
            this.ucb_archiveType.Text = x;
        });
    }

    private bool m_isCheckingPwd = false;

    [DllImport("kernel32.dll")]
    private static extern uint GetCurrentThreadId();

    /// <summary>
    /// ��̨�������뱾
    ///     ��������
    /// </summary>
    /// <param name="sender"></param>
    /// <param name="e"></param>
    private void ub_tryGetPwd_Click(object sender, EventArgs e)
    {
        if(!File.Exists(this.FilePath)) {
            this.AddMsgLine("�ļ�������");
            return;
        }

        if(this.m_isCheckingPwd) {
            return;
        }
        this.m_isCheckingPwd = true;
        this.ub_tryGetPwd.Enabled = false;
        string type = this.ucb_archiveType.Text;
        string format = this.utb_format.Text;
        //ArchiveToolCLR.
        // �����첽����
        Task task = Task.Run(async () =>
        {
            uint threadId = GetCurrentThreadId();
            this.AddMsgLine("��ʼ�����߳�id == " + threadId.ToString());
            List<String> pwds = PwdManagerCLR.GetAllPasswords();
            int size = pwds.Count;
            for(int i = 0; i < size; i++) {
                String pwd = pwds[i];
                this.utb_curPwd.Invoke(() => { this.utb_curPwd.Text = pwd; });
                this.AddMsgLine((i + 1).ToString() + "/" + size.ToString() + "::" + pwd);
                if(string.IsNullOrEmpty(pwd)) {
                    continue;
                }
                if(!ArchiveToolCLR.ArchiveExtraTestCLR(this.FilePath, pwd, type)) {
                    continue;
                }
                this.AddMsgLine("�ɹ�");
                break;
            }
            this.AddMsgLine("���");
            this.InvokeCall(() =>
            {
                this.m_isCheckingPwd = false;
                this.ub_tryGetPwd.Enabled = true;
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

    private void MainUI_DragEnter(object sender, DragEventArgs e)
    {
        // �������������Ƿ����ļ�
        if(e.Data.GetDataPresent(DataFormats.FileDrop)) {
            e.Effect = DragDropEffects.Copy; // �����Ϸ�Ч��
        }
        else {
            e.Effect = DragDropEffects.None; // ��ֹ�Ϸ�
        }
    }

    /// <summary>
    /// ��ק����ʱ������һ���ļ���ʼ��
    /// </summary>
    /// <param name="sender"></param>
    /// <param name="e"></param>
    private void MainUI_DragDrop(object sender, DragEventArgs e)
    {
        string[] files = (string[])e.Data.GetData(DataFormats.FileDrop);

        string file = files[0];
        this.toolStripStatusLabel1.Text = file;

        this.InitialByFile(file);
    }

    private void utb_archiveHeader_TextChanged(object sender, EventArgs e)
    {

    }
    /// <summary>
    /// �����Ϣ-msgBox��tooltrip
    /// </summary>
    /// <param name="msg"></param>
    /// <param name="byInvoke"></param>
    private void AddMsgLine(string msg, bool byInvoke = false)
    {
        if(this.InvokeRequired) {
            lock(this.MsgLinesBuffer) {
                this.MsgLinesBuffer.AppendLine(msg);
                this.MsgLineBuffer = msg;
                this.BeginInvoke(this.AddMsgLine, new object[] { msg, true });
                return;
            }
        }

        if(byInvoke) {
            if(this.MsgLinesBuffer.Length > 0) {
                this.utb_msg.Text += this.MsgLinesBuffer.ToString();
                this.MsgLinesBuffer.Clear();
            }
            if(this.MsgLineBuffer.Length > 0) {
                this.toolStripStatusLabel1.Text = this.MsgLineBuffer;
                this.MsgLineBuffer = "";
            }
            return;
        }
        else {
            this.toolStripStatusLabel1.Text = msg;
            this.utb_msg.Text += msg + Environment.NewLine;
            return;
        }

    }
    private string MsgLineBuffer { get; set; }
    private StringBuilder MsgLinesBuffer { get; set; } = new();


    /// <summary>
    /// ���ļ���ʼ��
    /// </summary>
    /// <param name="filePath"></param>
    private void InitialByFile(string filePath)
    {
        if(string.IsNullOrEmpty(filePath)) {
            return;
        }
        if(Directory.Exists(filePath)) {
            this.AddMsgLine("input file is a dir path :: " + filePath);
            return;
        }
        if(!File.Exists(filePath)) {
            this.AddMsgLine("file Not Exist :: " + filePath);
            return;
        }

        this.FilePath = filePath;
        string format = ArchiveToolCLR.CheckFormatCLR(this.FilePath);
        this.utb_format.Text = format;
        this.ucb_archiveType.Text = "Auto";
        for(int i = 0; i < this.ucb_archiveType.Items.Count; i++) {
            string itemStr = this.ucb_archiveType.Items[i]?.ToString() ?? "";
            if(itemStr.Equals(format, StringComparison.OrdinalIgnoreCase)) {
                this.ucb_archiveType.Text = itemStr;
                break;
            }
        }
    }

    private void ucb_archiveType_SelectedIndexChanged(object sender, EventArgs e)
    {

    }

    /// <summary>
    /// �������������ı������а�
    /// </summary>
    /// <param name="sender"></param>
    /// <param name="e"></param>
    private void ub_cpPwd_Click(object sender, EventArgs e)
    {
        string pwd = this.utb_curPwd.Text;
        Clipboard.SetText(pwd);
    }

    private void button1_Click(object sender, EventArgs e)
    {
        this.ub_tryGetPwd.Enabled = true;
        this.m_isCheckingPwd = false;
    }
}
