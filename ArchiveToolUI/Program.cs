namespace ArchiveToolUI;
using NS_ArchiveToolCLR;

//using ClassLibrary2;
static class Program
{
    /// <summary>
    ///  The main entry point for the application.
    /// </summary>
    [STAThread]
    static void Main( string[] args )
    {
        // To customize application configuration such as set high DPI settings or default font,
        // see https://aka.ms/applicationconfiguration.
        //ArchiveToolCLR.CheckFormatCLR("");
        //ArchiveToolCLR.test();
        ApplicationConfiguration.Initialize();
        MainUI ui = new();
        if ( args.Length > 0 )
        {
            ui.FilePath = args[0];
        }
        Application.Run(ui);
    }
}