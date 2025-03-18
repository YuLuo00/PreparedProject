namespace RawMoveUI
{
    partial class Form1
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
            if(disposing && (components != null)) {
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
            components = new System.ComponentModel.Container();
            dataGridView1 = new DataGridView();
            statusStrip1 = new StatusStrip();
            toolStripStatusLabel1 = new ToolStripStatusLabel();
            u_b_raw2StoreDir = new Button();
            toolTip1 = new ToolTip(components);
            bindingSource1 = new BindingSource(components);
            u_tb_rootDirPath = new TextBox();
            u_tb_rawStorePath = new TextBox();
            u_b_raw2PicLoc = new Button();
            openFileDialog1 = new OpenFileDialog();
            folderBrowserDialog1 = new FolderBrowserDialog();
            ((System.ComponentModel.ISupportInitialize)dataGridView1).BeginInit();
            statusStrip1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)bindingSource1).BeginInit();
            SuspendLayout();
            // 
            // dataGridView1
            // 
            dataGridView1.Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right;
            dataGridView1.ColumnHeadersHeightSizeMode = DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            dataGridView1.Location = new Point(12, 92);
            dataGridView1.Name = "dataGridView1";
            dataGridView1.Size = new Size(546, 215);
            dataGridView1.TabIndex = 0;
            // 
            // statusStrip1
            // 
            statusStrip1.Items.AddRange(new ToolStripItem[] { toolStripStatusLabel1 });
            statusStrip1.Location = new Point(0, 310);
            statusStrip1.Name = "statusStrip1";
            statusStrip1.Size = new Size(570, 22);
            statusStrip1.TabIndex = 1;
            statusStrip1.Text = "statusStrip1";
            // 
            // toolStripStatusLabel1
            // 
            toolStripStatusLabel1.Name = "toolStripStatusLabel1";
            toolStripStatusLabel1.Size = new Size(131, 17);
            toolStripStatusLabel1.Text = "toolStripStatusLabel1";
            // 
            // u_b_raw2StoreDir
            // 
            u_b_raw2StoreDir.Anchor = AnchorStyles.Top | AnchorStyles.Right;
            u_b_raw2StoreDir.Location = new Point(445, 12);
            u_b_raw2StoreDir.Name = "u_b_raw2StoreDir";
            u_b_raw2StoreDir.Size = new Size(113, 23);
            u_b_raw2StoreDir.TabIndex = 2;
            u_b_raw2StoreDir.Text = "Raw2StoreDir";
            u_b_raw2StoreDir.UseVisualStyleBackColor = true;
            u_b_raw2StoreDir.Click += Button1_Click;
            // 
            // u_tb_rootDirPath
            // 
            u_tb_rootDirPath.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
            u_tb_rootDirPath.Location = new Point(12, 12);
            u_tb_rootDirPath.Name = "u_tb_rootDirPath";
            u_tb_rootDirPath.PlaceholderText = "root dir";
            u_tb_rootDirPath.Size = new Size(427, 23);
            u_tb_rootDirPath.TabIndex = 3;
            u_tb_rootDirPath.DoubleClick += RootDirPath_DoubleClick;
            // 
            // u_tb_rawStorePath
            // 
            u_tb_rawStorePath.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
            u_tb_rawStorePath.Location = new Point(12, 41);
            u_tb_rawStorePath.Name = "u_tb_rawStorePath";
            u_tb_rawStorePath.PlaceholderText = "raw store dir";
            u_tb_rawStorePath.Size = new Size(427, 23);
            u_tb_rawStorePath.TabIndex = 4;
            u_tb_rawStorePath.DoubleClick += Tb_rawStorePath_DoubleClick;
            // 
            // u_b_raw2PicLoc
            // 
            u_b_raw2PicLoc.Anchor = AnchorStyles.Top | AnchorStyles.Right;
            u_b_raw2PicLoc.Location = new Point(445, 41);
            u_b_raw2PicLoc.Name = "u_b_raw2PicLoc";
            u_b_raw2PicLoc.Size = new Size(113, 23);
            u_b_raw2PicLoc.TabIndex = 2;
            u_b_raw2PicLoc.Text = "Raw2PicLocation";
            u_b_raw2PicLoc.UseVisualStyleBackColor = true;
            u_b_raw2PicLoc.Click += Button1_Click;
            // 
            // openFileDialog1
            // 
            openFileDialog1.FileName = "openFileDialog1";
            // 
            // Form1
            // 
            AutoScaleDimensions = new SizeF(7F, 17F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(570, 332);
            Controls.Add(u_tb_rawStorePath);
            Controls.Add(u_tb_rootDirPath);
            Controls.Add(u_b_raw2PicLoc);
            Controls.Add(u_b_raw2StoreDir);
            Controls.Add(statusStrip1);
            Controls.Add(dataGridView1);
            Margin = new Padding(2, 3, 2, 3);
            Name = "Form1";
            Text = "Form1";
            ((System.ComponentModel.ISupportInitialize)dataGridView1).EndInit();
            statusStrip1.ResumeLayout(false);
            statusStrip1.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)bindingSource1).EndInit();
            ResumeLayout(false);
            PerformLayout();
        }

        #endregion

        private DataGridView dataGridView1;
        private StatusStrip statusStrip1;
        private ToolStripStatusLabel toolStripStatusLabel1;
        private Button u_b_raw2StoreDir;
        private ToolTip toolTip1;
        private BindingSource bindingSource1;
        private TextBox u_tb_rootDirPath;
        private TextBox u_tb_rawStorePath;
        private Button u_b_raw2PicLoc;
        private OpenFileDialog openFileDialog1;
        private FolderBrowserDialog folderBrowserDialog1;
    }
}
