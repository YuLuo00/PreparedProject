namespace ArchiveToolUI;
using System;
using System.Runtime.InteropServices;

//using ClassLibrary2;
using NS_ArchiveToolCLR;

using System.Text;
using System.Runtime.CompilerServices;
using System.Reflection;
using System.Windows.Forms;
using static System.Windows.Forms.DataFormats;

public partial class MainUI : Form
{
    public MainUI()
    {
        InitializeComponent();
        this.AllowDrop = true;
        this.DoubleBuffered = true;
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
        PwdManagerCLR.AddPws(newPwd);
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
    /// 后台遍历密码本
    ///     不可重入
    /// </summary>
    /// <param name="sender"></param>
    /// <param name="e"></param>
    private void ub_tryGetPwd_Click(object sender, EventArgs e)
    {
        if(!File.Exists(this.FilePath)) {
            this.AddMsgLine("文件不存在");
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
        // 启动异步任务
        Task task = new Task(async () =>
        {
            uint threadId = GetCurrentThreadId();
            this.AddMsgLine("开始任务，线程id == " + threadId.ToString());
            List<String> pwds = PwdManagerCLR.GetAllPasswords();
            int size = pwds.Count;
            bool success = false;
            List<string> correctPwd = new();
            for(int i = 0; i < size; i++) {
                String pwd = pwds[i];
                this.Invoke(() => { this.ucb_pwdRet.Text = pwd; });
                this.AddMsgLine((i + 1).ToString() + "/" + size.ToString() + "::" + pwd);
                if(string.IsNullOrEmpty(pwd)) {
                    continue;
                }
                if(!ArchiveToolCLR.ArchiveExtraTestCLR(this.FilePath, pwd, type)) {
                    continue;
                }
                success = true;
                this.AddMsgLine("成功" + (i + 1).ToString() + "/" + size.ToString());
                correctPwd.Add(pwd);
                if(ucb_tryAll.Checked) {
                    continue;
                }
                break;
            }
            if(!success) {
                this.AddMsgLine("失败");
            }
            this.InvokeCall(() =>
            {
                this.m_isCheckingPwd = false;
                this.ub_tryGetPwd.Enabled = true;
                this.ucb_pwdRet.Items.Clear();
                foreach(String pwd in correctPwd) {
                    this.ucb_pwdRet.Text = pwd;
                    this.ucb_pwdRet.Items.Add(pwd);
                }
                this.AddMsgLine($"检索完成，有{correctPwd.Count}个匹配密码");
            });
        });
        task.ContinueWith(t => {

        });
        task.Start();
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

    private void MainUI_DragEnter(object sender, DragEventArgs e)
    {
        // 检查拖入的数据是否是文件
        if(e.Data.GetDataPresent(DataFormats.FileDrop)) {
            e.Effect = DragDropEffects.Copy; // 设置拖放效果
        }
        else {
            e.Effect = DragDropEffects.None; // 禁止拖放
        }
    }

    /// <summary>
    /// 拖拽进来时，按第一个文件初始化
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
    /// 添加信息-msgBox，tooltrip
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
    /// 按文件初始化
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
        this.SetArchiveTypeUcbByHeader(format);
    }

    private void SetArchiveTypeUcbByHeader(string header)
    {
        if(this.Header2Type.ContainsKey(header)) {
            this.ucb_archiveType.Text = this.Header2Type[header];
            return;
        }

        this.ucb_archiveType.Text = "Auto";
        for(int i = 0; i < this.ucb_archiveType.Items.Count; i++) {
            string itemStr = this.ucb_archiveType.Items[i]?.ToString() ?? "";
            if(itemStr.Equals(header, StringComparison.OrdinalIgnoreCase)) {
                this.ucb_archiveType.Text = itemStr;
                break;
            }
        }
    }

    private Dictionary<string, string> Header2Type { get; set; } = new(StringComparer.OrdinalIgnoreCase)
    {
        {"7zip","sevenzip" },
        {"7-zip","sevenzip" },
        {"ZIP 2.0 (deflation)","zip" },
    };

    private void ucb_archiveType_SelectedIndexChanged(object sender, EventArgs e)
    {

    }

    /// <summary>
    /// 拷贝密码栏的文本到剪切板
    /// </summary>
    /// <param name="sender"></param>
    /// <param name="e"></param>
    private void ub_cpPwd_Click(object sender, EventArgs e)
    {
        string pwd = this.ucb_pwdRet.Text;
        Clipboard.SetText(pwd);
    }

    private void button1_Click(object sender, EventArgs e)
    {
        this.ub_tryGetPwd.Enabled = true;
        this.m_isCheckingPwd = false;
    }

    private void utb_format_DoubleClick(object sender, EventArgs e)
    {
        if(this.IsAutoDerminingType) {
            this.AddMsgLine("有后台判定任务正在执行，此次判定无法执行");
            return;
        }
        if(!File.Exists(this.FilePath)) {
            return;
        }
        this.IsAutoDerminingType = true;
        this.AddMsgLine("双击类型框，开始尝试自动判定");

        Task task = new Task(async () =>
        {
            string type = "";
            try {
                type = ArchiveToolCLR.TryDetermineTypeCLR(this.FilePath);
            }
            catch(Exception ex) {
                this.AddMsgLine($"{ex.Message}");
            }
            if(string.IsNullOrEmpty(type)) {
                this.AddMsgLine("判定失败");
                return;
            }

            string archiveToolMsg = ArchiveToolCLR.Msg();
            this.AddMsgLine(archiveToolMsg);
            this.AddMsgLine("自动判定完成，结果::" + type);
            this.Invoke(() => { this.SetArchiveTypeUcbByHeader(type); });
            this.IsAutoDerminingType = false;
        });
        task.ContinueWith(t => {
            this.IsAutoDerminingType = false;
        });

        task.Start();
    }

    private void utb_msg_MouseDown(object sender, MouseEventArgs e)
    {
        bool left = e.Button == MouseButtons.Left;
    }

    private bool IsAutoDerminingType { get; set; } = false;
}
