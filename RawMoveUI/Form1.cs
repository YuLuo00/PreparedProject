namespace RawMoveUI
{
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();
        }

        private void Button1_Click(object sender, EventArgs e)
        {

        }

        /// <summary>
        /// 双击
        /// 选一个文件夹作为root dir
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void RootDirPath_DoubleClick(object sender, EventArgs e)
        {
            folderBrowserDialog1.Description = "请选择一个文件夹";
            folderBrowserDialog1.SelectedPath = Environment.GetFolderPath(Environment.SpecialFolder.Desktop); // 默认打开路径
            if(Directory.Exists(this.u_tb_rootDirPath.Text)) {
                folderBrowserDialog1.SelectedPath = this.u_tb_rootDirPath.Text;
            }

            if(folderBrowserDialog1.ShowDialog() == DialogResult.OK) {
                string folderPath = folderBrowserDialog1.SelectedPath;
                this.u_tb_rootDirPath.Text = folderPath;
                Console.WriteLine($"选择的文件夹: {folderPath}");
            }
            else {
                Console.WriteLine("未选择文件夹");
            }
        }

        /// <summary>
        /// 双击
        /// 选一个文件夹作为raw store
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void Tb_rawStorePath_DoubleClick(object sender, EventArgs e)
        {
            folderBrowserDialog1.Description = "请选择一个文件夹";
            folderBrowserDialog1.SelectedPath = Environment.GetFolderPath(Environment.SpecialFolder.Desktop); // 默认打开路径
            if(Directory.Exists(this.u_tb_rawStorePath.Text)) {
                folderBrowserDialog1.SelectedPath = this.u_tb_rawStorePath.Text;
            }
            else if(Directory.Exists(this.u_tb_rootDirPath.Text)) {
                folderBrowserDialog1.SelectedPath = this.u_tb_rootDirPath.Text;
            }

            if(folderBrowserDialog1.ShowDialog() == DialogResult.OK) {
                string folderPath = folderBrowserDialog1.SelectedPath;
                this.u_tb_rootDirPath.Text = folderPath;
                Console.WriteLine($"选择的文件夹: {folderPath}");
            }
            else {
                Console.WriteLine("未选择文件夹");
            }
        }
    }
}
