namespace ArchiveToolUI;

partial class MainUI
{
    /// <summary>
    ///  Required designer variable.
    /// </summary>
    private System.ComponentModel.IContainer components = null;

    /// <summary>
    ///  Clean up any resources being used.
    /// </summary>
    /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
    protected override void Dispose(bool disposing)
    {
        if (disposing && (components != null))
        {
            components.Dispose();
        }
        base.Dispose(disposing);
    }

    #region Windows Form Designer generated code

    /// <summary>
    ///  Required method for Designer support - do not modify
    ///  the contents of this method with the code editor.
    /// </summary>
    private void InitializeComponent()
    {
        utb_newPwd = new TextBox();
        ub_addNewPwd = new Button();
        utb_curPwd = new TextBox();
        ub_tryGetPwd = new Button();
        utb_msg = new TextBox();
        u_statusStrip = new StatusStrip();
        toolStripStatusLabel1 = new ToolStripStatusLabel();
        ucb_archiveType = new ComboBox();
        utb_format = new TextBox();
        ub_cpPwd = new Button();
        button1 = new Button();
        u_statusStrip.SuspendLayout();
        SuspendLayout();
        // 
        // utb_newPwd
        // 
        utb_newPwd.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
        utb_newPwd.Location = new Point(17, 13);
        utb_newPwd.Name = "utb_newPwd";
        utb_newPwd.PlaceholderText = "添加新密码到密码本";
        utb_newPwd.Size = new Size(327, 23);
        utb_newPwd.TabIndex = 0;
        // 
        // ub_addNewPwd
        // 
        ub_addNewPwd.Anchor = AnchorStyles.Top | AnchorStyles.Right;
        ub_addNewPwd.Location = new Point(366, 13);
        ub_addNewPwd.Name = "ub_addNewPwd";
        ub_addNewPwd.Size = new Size(91, 23);
        ub_addNewPwd.TabIndex = 1;
        ub_addNewPwd.Text = "添加新密码";
        ub_addNewPwd.UseVisualStyleBackColor = true;
        ub_addNewPwd.Click += ub_addNewPwd_Click;
        // 
        // utb_curPwd
        // 
        utb_curPwd.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
        utb_curPwd.Location = new Point(17, 42);
        utb_curPwd.Name = "utb_curPwd";
        utb_curPwd.PlaceholderText = "当前在检查的密码";
        utb_curPwd.Size = new Size(294, 23);
        utb_curPwd.TabIndex = 2;
        // 
        // ub_tryGetPwd
        // 
        ub_tryGetPwd.Anchor = AnchorStyles.Top | AnchorStyles.Right;
        ub_tryGetPwd.Location = new Point(366, 42);
        ub_tryGetPwd.Name = "ub_tryGetPwd";
        ub_tryGetPwd.Size = new Size(91, 23);
        ub_tryGetPwd.TabIndex = 3;
        ub_tryGetPwd.Text = "检查密码";
        ub_tryGetPwd.UseVisualStyleBackColor = true;
        ub_tryGetPwd.Click += ub_tryGetPwd_Click;
        // 
        // utb_msg
        // 
        utb_msg.Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right;
        utb_msg.Location = new Point(18, 70);
        utb_msg.Multiline = true;
        utb_msg.Name = "utb_msg";
        utb_msg.ScrollBars = ScrollBars.Both;
        utb_msg.Size = new Size(329, 116);
        utb_msg.TabIndex = 4;
        utb_msg.MouseDown += utb_msg_MouseDown;
        // 
        // u_statusStrip
        // 
        u_statusStrip.Items.AddRange(new ToolStripItem[] { toolStripStatusLabel1 });
        u_statusStrip.Location = new Point(0, 198);
        u_statusStrip.Name = "u_statusStrip";
        u_statusStrip.Size = new Size(469, 22);
        u_statusStrip.TabIndex = 5;
        u_statusStrip.Text = "statusStrip1";
        // 
        // toolStripStatusLabel1
        // 
        toolStripStatusLabel1.Name = "toolStripStatusLabel1";
        toolStripStatusLabel1.Size = new Size(131, 17);
        toolStripStatusLabel1.Text = "toolStripStatusLabel1";
        // 
        // ucb_archiveType
        // 
        ucb_archiveType.Anchor = AnchorStyles.Top | AnchorStyles.Right;
        ucb_archiveType.FormattingEnabled = true;
        ucb_archiveType.Location = new Point(366, 109);
        ucb_archiveType.Name = "ucb_archiveType";
        ucb_archiveType.Size = new Size(91, 25);
        ucb_archiveType.TabIndex = 6;
        ucb_archiveType.SelectedIndexChanged += ucb_archiveType_SelectedIndexChanged;
        // 
        // utb_format
        // 
        utb_format.Anchor = AnchorStyles.Top | AnchorStyles.Right;
        utb_format.Location = new Point(366, 77);
        utb_format.Name = "utb_format";
        utb_format.PlaceholderText = "文件格式";
        utb_format.Size = new Size(91, 23);
        utb_format.TabIndex = 7;
        utb_format.TextChanged += utb_archiveHeader_TextChanged;
        utb_format.DoubleClick += utb_format_DoubleClick;
        // 
        // ub_cpPwd
        // 
        ub_cpPwd.Anchor = AnchorStyles.Top | AnchorStyles.Right;
        ub_cpPwd.Location = new Point(311, 42);
        ub_cpPwd.Name = "ub_cpPwd";
        ub_cpPwd.Size = new Size(36, 23);
        ub_cpPwd.TabIndex = 8;
        ub_cpPwd.Text = "cp";
        ub_cpPwd.UseVisualStyleBackColor = true;
        ub_cpPwd.Click += ub_cpPwd_Click;
        // 
        // button1
        // 
        button1.Anchor = AnchorStyles.Top | AnchorStyles.Right;
        button1.Location = new Point(366, 140);
        button1.Name = "button1";
        button1.Size = new Size(91, 23);
        button1.TabIndex = 9;
        button1.Text = "button1";
        button1.UseVisualStyleBackColor = true;
        button1.Click += button1_Click;
        // 
        // MainUI
        // 
        AutoScaleDimensions = new SizeF(7F, 17F);
        AutoScaleMode = AutoScaleMode.Font;
        ClientSize = new Size(469, 220);
        Controls.Add(button1);
        Controls.Add(ub_cpPwd);
        Controls.Add(utb_format);
        Controls.Add(ucb_archiveType);
        Controls.Add(u_statusStrip);
        Controls.Add(utb_msg);
        Controls.Add(ub_tryGetPwd);
        Controls.Add(utb_curPwd);
        Controls.Add(ub_addNewPwd);
        Controls.Add(utb_newPwd);
        Margin = new Padding(2, 3, 2, 3);
        Name = "MainUI";
        Text = "Form1";
        Load += MainUI_Load;
        DragDrop += MainUI_DragDrop;
        DragEnter += MainUI_DragEnter;
        u_statusStrip.ResumeLayout(false);
        u_statusStrip.PerformLayout();
        ResumeLayout(false);
        PerformLayout();
    }

    #endregion

    private TextBox utb_newPwd;
    private Button ub_addNewPwd;
    private TextBox utb_curPwd;
    private Button ub_tryGetPwd;
    private TextBox utb_msg;
    private StatusStrip u_statusStrip;
    private ToolStripStatusLabel toolStripStatusLabel1;
    private ComboBox ucb_archiveType;
    private TextBox utb_format;
    private Button ub_cpPwd;
    private Button button1;
}
