   M S B A M L   `   `   `  ???? N  JUnrealBinaryBuilder, Version=3.1.6.0, Culture=neutral, PublicKeyToken=nullS.clr-namespace:UnrealBinaryBuilder.UserControls UnrealBinaryBuilder.UserControls  0    *UnrealBinaryBuilder.UserControls.LogViewer   W SPresentationCore, Version=4.0.0.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35\ XPresentationFramework, Version=4.0.0.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35e aSystem.Windows.Controls.Ribbon, Version=4.0.0.0, Culture=neutral, PublicKeyToken=b77a5c561934e089R NWindowsBase, Version=4.0.0.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35^ ZWindowsFormsIntegration, Version=4.0.0.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35H 9http://schemas.microsoft.com/winfx/2006/xaml/presentation      R NSystem.Xaml, Version=4.0.0.0, Culture=neutral, PublicKeyToken=b77a5c561934e089:x,http://schemas.microsoft.com/winfx/2006/xaml     Jmc;http://schemas.openxmlformats.org/markup-compatibility/2006     @d2http://schemas.microsoft.com/expression/blend/2008     @local.clr-namespace:UnrealBinaryBuilder.UserControls       ? 	Resources   ?? 
TargetType   LogViewerStyle ?? Property ?? Value K? IsItemsHost ?? DataType/   )UnrealBinaryBuilder.UserControls.LogEntry ? IsSharedSizeScope ? ColumnDefinitions z? SharedSizeGroup	 ??Name

 ? Tag??%6  &	        (?? g     *, +)?? " ??.???  ? ?? ?? .R??? $
??True??.???? ??  ? ?? ?? .R?$? $
 True???? "  .R?? $
 True?? ?? $
 Date??$
??Auto ??? $ Message??$
??Auto ??? .+??? $	 CopyBtn??-   $??0??$
??Copy??$??50??$??20??$??0q?$??0 5 0 0q?$??11(?y??? *MsgVisibility+
 ?? *	Message+?? $??1??$
??Bold"?$??5 5 10 0q?$??Center=????? *
DateTime+y??? *MsgVisibility+?? $??2??$
??Wrap=?$??5 0 0 0q?$??Center=????? *	Message+???? *MessageColor+.??]? .+??? $??Bottom=?$??5,0,5,0q?$y?Visible=????? *Count+ ?? StringFormat$ Total log entries: {0}??? .+??? ???	#FF111111?? #??[  $??10q????? ???? .R??? -   $
??True??.???? ???? .R?$? $
 True??