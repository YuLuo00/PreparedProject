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
            this.txtFilePath   = new System.Windows.Forms.TextBox();
            this.cmbType       = new System.Windows.Forms.ComboBox();
            this.txtResult     = new System.Windows.Forms.TextBox();
            this.btnCopy       = new DropdownButton();
            this.toolTip       = new System.Windows.Forms.ToolTip();
            this.txtFilePaths  = new System.Windows.Forms.TextBox();
            this.lvResults     = new System.Windows.Forms.ListView();
            this.colFile       = new System.Windows.Forms.ColumnHeader();
            this.colPwd        = new System.Windows.Forms.ColumnHeader();
            this.colType       = new System.Windows.Forms.ColumnHeader();
            this.chkFindAll    = new System.Windows.Forms.CheckBox();
            this.chkBatchMode  = new System.Windows.Forms.CheckBox();
            this.btnStart      = new System.Windows.Forms.Button();
            this.ctxBtnCopy    = new System.Windows.Forms.ContextMenuStrip();
            this.menuAddPwd    = new System.Windows.Forms.ToolStripMenuItem();
            this.ctxBtnCopy.SuspendLayout();
            this.statusStrip   = new System.Windows.Forms.StatusStrip();
            this.tsProgressBar = new System.Windows.Forms.ToolStripProgressBar();
            this.tsLabel       = new System.Windows.Forms.ToolStripStatusLabel();
            this.tsTypeLabel   = new System.Windows.Forms.ToolStripStatusLabel();
            this.statusStrip.SuspendLayout();
            this.SuspendLayout();

            // ── 窗体 ──────────────────────────────────────────────────
            this.ClientSize      = new System.Drawing.Size(400, 158);
            this.MinimumSize     = new System.Drawing.Size(320, 190);
            this.Text            = "ArchiveTool";
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.Sizable;
            this.MaximizeBox     = true;
            this.AllowDrop       = true;
            this.Name            = "Form1";

            // ── 单文件：地址栏（左右拉伸）────────────────────────────
            this.txtFilePath.Location  = new System.Drawing.Point(8, 10);
            this.txtFilePath.Size      = new System.Drawing.Size(258, 23);
            this.txtFilePath.Anchor    = System.Windows.Forms.AnchorStyles.Top
                                       | System.Windows.Forms.AnchorStyles.Left
                                       | System.Windows.Forms.AnchorStyles.Right;
            this.txtFilePath.AllowDrop = true;
            this.txtFilePath.Name      = "txtFilePath";
            this.txtFilePath.TabIndex  = 0;

            // ── 单文件：类型下拉框（右对齐）──────────────────────────
            this.cmbType.Location      = new System.Drawing.Point(270, 10);
            this.cmbType.Size          = new System.Drawing.Size(122, 23);
            this.cmbType.Anchor        = System.Windows.Forms.AnchorStyles.Top
                                       | System.Windows.Forms.AnchorStyles.Right;
            this.cmbType.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.cmbType.Name          = "cmbType";
            this.cmbType.TabIndex      = 1;

            // ── 单文件：结果框（左右拉伸）────────────────────────────
            this.txtResult.Location    = new System.Drawing.Point(8, 65);
            this.txtResult.Size        = new System.Drawing.Size(290, 23);
            this.txtResult.Anchor      = System.Windows.Forms.AnchorStyles.Top
                                       | System.Windows.Forms.AnchorStyles.Left
                                       | System.Windows.Forms.AnchorStyles.Right;
            this.txtResult.ReadOnly    = true;
            this.txtResult.Name        = "txtResult";
            this.txtResult.TabIndex    = 4;

            // ── 单文件：拷贝按钮（右对齐）────────────────────────────
            this.menuAddPwd.Text = "添加密码到密码本";
            this.menuAddPwd.Name = "menuAddPwd";
            this.ctxBtnCopy.Items.Add(this.menuAddPwd);
            this.ctxBtnCopy.Name = "ctxBtnCopy";

            this.btnCopy.Location         = new System.Drawing.Point(302, 65);
            this.btnCopy.Size             = new System.Drawing.Size(90, 23);
            this.btnCopy.Anchor           = System.Windows.Forms.AnchorStyles.Top
                                          | System.Windows.Forms.AnchorStyles.Right;
            this.btnCopy.Text             = "拷贝密码";
            this.btnCopy.Name             = "btnCopy";
            this.btnCopy.TabIndex         = 5;
            this.btnCopy.ContextMenuStrip = this.ctxBtnCopy;
            this.toolTip.SetToolTip(this.btnCopy, "左键：拷贝密码\n右键：添加密码到密码本");

            // ── 批量：多行地址栏（左右拉伸）──────────────────────────
            this.txtFilePaths.Location   = new System.Drawing.Point(8, 10);
            this.txtFilePaths.Size       = new System.Drawing.Size(384, 80);
            this.txtFilePaths.Anchor     = System.Windows.Forms.AnchorStyles.Top
                                         | System.Windows.Forms.AnchorStyles.Left
                                         | System.Windows.Forms.AnchorStyles.Right;
            this.txtFilePaths.Multiline  = true;
            this.txtFilePaths.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
            this.txtFilePaths.AllowDrop  = true;
            this.txtFilePaths.Visible    = false;
            this.txtFilePaths.Name       = "txtFilePaths";
            this.txtFilePaths.TabIndex   = 0;

            // ── 批量：结果列表（四边拉伸）────────────────────────────
            this.colFile.Text  = "文件";
            this.colFile.Width = 160;
            this.colPwd.Text   = "密码";
            this.colPwd.Width  = 130;
            this.colType.Text  = "类型";
            this.colType.Width = 80;

            this.lvResults.Location      = new System.Drawing.Point(8, 122);
            this.lvResults.Size          = new System.Drawing.Size(384, 120);
            this.lvResults.Anchor        = System.Windows.Forms.AnchorStyles.Top
                                         | System.Windows.Forms.AnchorStyles.Left
                                         | System.Windows.Forms.AnchorStyles.Right;
            this.lvResults.View          = System.Windows.Forms.View.Details;
            this.lvResults.FullRowSelect = true;
            this.lvResults.GridLines     = true;
            this.lvResults.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
                this.colFile, this.colPwd, this.colType });
            this.lvResults.Visible       = false;
            this.lvResults.Name          = "lvResults";
            this.lvResults.TabIndex      = 4;

            // ── 共用：勾选框行（左对齐）──────────────────────────────
            this.chkFindAll.Location   = new System.Drawing.Point(8, 40);
            this.chkFindAll.Size       = new System.Drawing.Size(160, 20);
            this.chkFindAll.Anchor     = System.Windows.Forms.AnchorStyles.Top
                                       | System.Windows.Forms.AnchorStyles.Left;
            this.chkFindAll.Text       = "检查全部匹配密码";
            this.chkFindAll.Name       = "chkFindAll";
            this.chkFindAll.TabIndex   = 2;

            this.chkBatchMode.Location = new System.Drawing.Point(180, 40);
            this.chkBatchMode.Size     = new System.Drawing.Size(100, 20);
            this.chkBatchMode.Anchor   = System.Windows.Forms.AnchorStyles.Top
                                       | System.Windows.Forms.AnchorStyles.Left;
            this.chkBatchMode.Text     = "批量模式";
            this.chkBatchMode.Name     = "chkBatchMode";
            this.chkBatchMode.TabIndex = 3;

            // ── 共用：开始/停止按钮（左右拉伸，底部锚定）────────────
            this.btnStart.Location = new System.Drawing.Point(8, 94);
            this.btnStart.Size     = new System.Drawing.Size(384, 28);
            this.btnStart.Anchor   = System.Windows.Forms.AnchorStyles.Bottom
                                   | System.Windows.Forms.AnchorStyles.Left
                                   | System.Windows.Forms.AnchorStyles.Right;
            this.btnStart.Text     = "开始检索";
            this.btnStart.Font     = new System.Drawing.Font(this.btnStart.Font, System.Drawing.FontStyle.Bold);
            this.btnStart.Name     = "btnStart";
            this.btnStart.TabIndex = 6;

            // ── 共用：底部 StatusStrip ────────────────────────────────
            this.tsProgressBar.Name    = "tsProgressBar";
            this.tsProgressBar.Size    = new System.Drawing.Size(150, 16);
            this.tsProgressBar.Minimum = 0;
            this.tsProgressBar.Maximum = 100;

            this.tsTypeLabel.Name      = "tsTypeLabel";
            this.tsTypeLabel.Text      = "";
            this.tsTypeLabel.ForeColor = System.Drawing.Color.DimGray;

            this.tsLabel.Name      = "tsLabel";
            this.tsLabel.Text      = "就绪";
            this.tsLabel.Spring    = true;
            this.tsLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;

            this.statusStrip.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
                this.tsProgressBar, this.tsTypeLabel, this.tsLabel });
            this.statusStrip.Name     = "statusStrip";
            this.statusStrip.TabIndex = 7;

            // ── 添加控件 ──────────────────────────────────────────────
            this.Controls.Add(this.txtFilePath);
            this.Controls.Add(this.cmbType);
            this.Controls.Add(this.txtResult);
            this.Controls.Add(this.btnCopy);
            this.Controls.Add(this.txtFilePaths);
            this.Controls.Add(this.lvResults);
            this.Controls.Add(this.chkFindAll);
            this.Controls.Add(this.chkBatchMode);
            this.Controls.Add(this.btnStart);
            this.Controls.Add(this.statusStrip);

            this.ctxBtnCopy.ResumeLayout(false);
            this.statusStrip.ResumeLayout(false);
            this.statusStrip.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();
        }

        // 单文件模式
        private System.Windows.Forms.TextBox      txtFilePath;
        private System.Windows.Forms.ComboBox     cmbType;
        private System.Windows.Forms.TextBox      txtResult;
        private DropdownButton                         btnCopy;
        private System.Windows.Forms.ToolTip           toolTip;
        private System.Windows.Forms.ContextMenuStrip  ctxBtnCopy;
        private System.Windows.Forms.ToolStripMenuItem menuAddPwd;
        // 批量模式
        private System.Windows.Forms.TextBox      txtFilePaths;
        private System.Windows.Forms.ListView     lvResults;
        private System.Windows.Forms.ColumnHeader colFile;
        private System.Windows.Forms.ColumnHeader colPwd;
        private System.Windows.Forms.ColumnHeader colType;
        // 共用
        private System.Windows.Forms.CheckBox          chkFindAll;
        private System.Windows.Forms.CheckBox          chkBatchMode;
        private System.Windows.Forms.Button            btnStart;
        private System.Windows.Forms.StatusStrip       statusStrip;
        private System.Windows.Forms.ToolStripProgressBar tsProgressBar;
        private System.Windows.Forms.ToolStripStatusLabel tsLabel;
        private System.Windows.Forms.ToolStripStatusLabel tsTypeLabel;
    }
}
