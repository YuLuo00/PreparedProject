namespace ArchiveToolUI
{
    partial class Form1
    {
        private System.ComponentModel.IContainer components = null;

        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null)) components.Dispose();
            base.Dispose(disposing);
        }

        private void InitializeComponent()
        {
            txtFilePath   = new TextBox();
            cmbType       = new ComboBox();
            chkFindAll    = new CheckBox();
            txtResult     = new TextBox();
            btnCopy       = new Button();
            btnStart      = new Button();
            progressBar   = new ProgressBar();
            lblStatus     = new Label();

            SuspendLayout();

            // ── 窗体 ──────────────────────────────────────────────────
            ClientSize    = new Size(400, 180);
            Text          = "ArchiveTool";
            FormBorderStyle = FormBorderStyle.FixedSingle;
            MaximizeBox   = false;
            AllowDrop     = true;

            // ── 行1：地址栏 + 类型下拉框 ─────────────────────────────
            txtFilePath.Location    = new Point(8, 10);
            txtFilePath.Size        = new Size(258, 23);
            txtFilePath.PlaceholderText = "拖入文件或输入路径…";
            txtFilePath.AllowDrop   = true;

            cmbType.Location        = new Point(270, 10);
            cmbType.Size            = new Size(122, 23);
            cmbType.DropDownStyle   = ComboBoxStyle.DropDownList;

            // ── 行2：勾选框 ───────────────────────────────────────────
            chkFindAll.Location     = new Point(8, 40);
            chkFindAll.Size         = new Size(200, 20);
            chkFindAll.Text         = "检查全部匹配密码";

            // ── 行3：结果框 + 拷贝按钮 ───────────────────────────────
            txtResult.Location      = new Point(8, 65);
            txtResult.Size          = new Size(290, 23);
            txtResult.ReadOnly      = true;
            txtResult.PlaceholderText = "匹配到的密码";

            btnCopy.Location        = new Point(302, 65);
            btnCopy.Size            = new Size(90, 23);
            btnCopy.Text            = "拷贝密码";

            // ── 行4：开始/停止按钮 ────────────────────────────────────
            btnStart.Location       = new Point(8, 94);
            btnStart.Size           = new Size(384, 28);
            btnStart.Text           = "开始检索";
            btnStart.Font           = new Font(btnStart.Font, FontStyle.Bold);

            // ── 行5：进度条 + 状态标签 ───────────────────────────────
            progressBar.Location    = new Point(8, 130);
            progressBar.Size        = new Size(280, 16);
            progressBar.Minimum     = 0;
            progressBar.Maximum     = 100;

            lblStatus.Location      = new Point(292, 130);
            lblStatus.Size          = new Size(100, 16);
            lblStatus.Text          = "就绪";
            lblStatus.TextAlign     = ContentAlignment.MiddleLeft;

            // ── 添加控件 ──────────────────────────────────────────────
            Controls.AddRange(new Control[] {
                txtFilePath, cmbType,
                chkFindAll,
                txtResult, btnCopy,
                btnStart,
                progressBar, lblStatus
            });

            ResumeLayout(false);
        }

        private TextBox   txtFilePath;
        private ComboBox  cmbType;
        private CheckBox  chkFindAll;
        private TextBox   txtResult;
        private Button    btnCopy;
        private Button    btnStart;
        private ProgressBar progressBar;
        private Label     lblStatus;
    }
}
