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
            // 单文件模式控件
            txtFilePath   = new TextBox();
            cmbType       = new ComboBox();
            txtResult     = new TextBox();
            btnCopy       = new Button();

            // 批量模式控件
            txtFilePaths  = new TextBox();
            lvResults     = new ListView();
            colFile       = new ColumnHeader();
            colPwd        = new ColumnHeader();

            // 共用控件
            chkFindAll    = new CheckBox();
            chkBatchMode  = new CheckBox();
            btnStart      = new Button();
            progressBar   = new ProgressBar();
            lblStatus     = new Label();

            SuspendLayout();

            // ── 窗体 ──────────────────────────────────────────────────
            ClientSize      = new Size(400, 180);
            Text            = "ArchiveTool";
            FormBorderStyle = FormBorderStyle.FixedSingle;
            MaximizeBox     = false;
            AllowDrop       = true;

            // ── 单文件：地址栏 ────────────────────────────────────────
            txtFilePath.Location      = new Point(8, 10);
            txtFilePath.Size          = new Size(258, 23);
            txtFilePath.PlaceholderText = "拖入文件或输入路径…";
            txtFilePath.AllowDrop     = true;

            // ── 单文件：类型下拉框 ────────────────────────────────────
            cmbType.Location          = new Point(270, 10);
            cmbType.Size              = new Size(122, 23);
            cmbType.DropDownStyle     = ComboBoxStyle.DropDownList;

            // ── 单文件：结果框 ────────────────────────────────────────
            txtResult.Location        = new Point(8, 65);
            txtResult.Size            = new Size(290, 23);
            txtResult.ReadOnly        = true;
            txtResult.PlaceholderText = "匹配到的密码";

            // ── 单文件：拷贝按钮 ──────────────────────────────────────
            btnCopy.Location          = new Point(302, 65);
            btnCopy.Size              = new Size(90, 23);
            btnCopy.Text              = "拷贝密码";

            // ── 批量：多行地址栏 ──────────────────────────────────────
            txtFilePaths.Location     = new Point(8, 10);
            txtFilePaths.Size         = new Size(384, 80);
            txtFilePaths.Multiline    = true;
            txtFilePaths.ScrollBars   = ScrollBars.Vertical;
            txtFilePaths.PlaceholderText = "拖入多个文件，或每行输入一个路径…";
            txtFilePaths.AllowDrop    = true;
            txtFilePaths.Visible      = false;

            // ── 批量：结果列表 ────────────────────────────────────────
            colFile.Text              = "文件";
            colFile.Width             = 200;
            colPwd.Text               = "密码";
            colPwd.Width              = 170;

            lvResults.Location        = new Point(8, 122);
            lvResults.Size            = new Size(384, 120);
            lvResults.View            = View.Details;
            lvResults.FullRowSelect   = true;
            lvResults.GridLines       = true;
            lvResults.Columns.AddRange(new[] { colFile, colPwd });
            lvResults.Visible         = false;

            // ── 共用：勾选框行 ────────────────────────────────────────
            chkFindAll.Location       = new Point(8, 40);
            chkFindAll.Size           = new Size(160, 20);
            chkFindAll.Text           = "检查全部匹配密码";

            chkBatchMode.Location     = new Point(180, 40);
            chkBatchMode.Size         = new Size(100, 20);
            chkBatchMode.Text         = "批量模式";

            // ── 共用：开始/停止按钮 ───────────────────────────────────
            btnStart.Location         = new Point(8, 94);
            btnStart.Size             = new Size(384, 28);
            btnStart.Text             = "开始检索";
            btnStart.Font             = new Font(btnStart.Font, FontStyle.Bold);

            // ── 共用：进度条 + 状态标签 ───────────────────────────────
            progressBar.Location      = new Point(8, 130);
            progressBar.Size          = new Size(280, 16);
            progressBar.Minimum       = 0;
            progressBar.Maximum       = 100;

            lblStatus.Location        = new Point(292, 130);
            lblStatus.Size            = new Size(100, 16);
            lblStatus.Text            = "就绪";
            lblStatus.TextAlign       = ContentAlignment.MiddleLeft;

            // ── 添加控件 ──────────────────────────────────────────────
            Controls.AddRange(new Control[] {
                txtFilePath, cmbType,
                txtResult, btnCopy,
                txtFilePaths, lvResults,
                chkFindAll, chkBatchMode,
                btnStart,
                progressBar, lblStatus
            });

            ResumeLayout(false);
        }

        // 单文件模式
        private TextBox     txtFilePath;
        private ComboBox    cmbType;
        private TextBox     txtResult;
        private Button      btnCopy;
        // 批量模式
        private TextBox     txtFilePaths;
        private ListView    lvResults;
        private ColumnHeader colFile;
        private ColumnHeader colPwd;
        // 共用
        private CheckBox    chkFindAll;
        private CheckBox    chkBatchMode;
        private Button      btnStart;
        private ProgressBar progressBar;
        private Label       lblStatus;
    }
}
