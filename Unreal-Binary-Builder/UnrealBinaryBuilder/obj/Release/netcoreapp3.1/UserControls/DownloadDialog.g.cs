﻿#pragma checksum "..\..\..\..\UserControls\DownloadDialog.xaml" "{ff1816ec-aa5e-4d10-87f7-6f4963833460}" "39FD786612C522F0CB406310DD6294C0E799CD8A"
//------------------------------------------------------------------------------
// <auto-generated>
//     This code was generated by a tool.
//     Runtime Version:4.0.30319.42000
//
//     Changes to this file may cause incorrect behavior and will be lost if
//     the code is regenerated.
// </auto-generated>
//------------------------------------------------------------------------------

using CefSharp.Wpf;
using System;
using System.Diagnostics;
using System.Windows;
using System.Windows.Automation;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Controls.Ribbon;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Forms.Integration;
using System.Windows.Ink;
using System.Windows.Input;
using System.Windows.Markup;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Media.Effects;
using System.Windows.Media.Imaging;
using System.Windows.Media.Media3D;
using System.Windows.Media.TextFormatting;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Windows.Shell;
using UnrealBinaryBuilder.UserControls;


namespace UnrealBinaryBuilder.UserControls {
    
    
    /// <summary>
    /// DownloadDialog
    /// </summary>
    public partial class DownloadDialog : System.Windows.Controls.Border, System.Windows.Markup.IComponentConnector {
        
        
        #line 12 "..\..\..\..\UserControls\DownloadDialog.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.TextBlock DownloadProgressTextBlock;
        
        #line default
        #line hidden
        
        
        #line 13 "..\..\..\..\UserControls\DownloadDialog.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.ProgressBar DownloadProgressbar;
        
        #line default
        #line hidden
        
        
        #line 14 "..\..\..\..\UserControls\DownloadDialog.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal CefSharp.Wpf.ChromiumWebBrowser CefWebBrowser;
        
        #line default
        #line hidden
        
        
        #line 16 "..\..\..\..\UserControls\DownloadDialog.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.Button DownloadNowBtn;
        
        #line default
        #line hidden
        
        
        #line 17 "..\..\..\..\UserControls\DownloadDialog.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.Button CancelBtn;
        
        #line default
        #line hidden
        
        private bool _contentLoaded;
        
        /// <summary>
        /// InitializeComponent
        /// </summary>
        [System.Diagnostics.DebuggerNonUserCodeAttribute()]
        [System.CodeDom.Compiler.GeneratedCodeAttribute("PresentationBuildTasks", "5.0.17.0")]
        public void InitializeComponent() {
            if (_contentLoaded) {
                return;
            }
            _contentLoaded = true;
            System.Uri resourceLocater = new System.Uri("/UnrealBinaryBuilder;component/usercontrols/downloaddialog.xaml", System.UriKind.Relative);
            
            #line 1 "..\..\..\..\UserControls\DownloadDialog.xaml"
            System.Windows.Application.LoadComponent(this, resourceLocater);
            
            #line default
            #line hidden
        }
        
        [System.Diagnostics.DebuggerNonUserCodeAttribute()]
        [System.CodeDom.Compiler.GeneratedCodeAttribute("PresentationBuildTasks", "5.0.17.0")]
        [System.ComponentModel.EditorBrowsableAttribute(System.ComponentModel.EditorBrowsableState.Never)]
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Design", "CA1033:InterfaceMethodsShouldBeCallableByChildTypes")]
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Maintainability", "CA1502:AvoidExcessiveComplexity")]
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1800:DoNotCastUnnecessarily")]
        void System.Windows.Markup.IComponentConnector.Connect(int connectionId, object target) {
            switch (connectionId)
            {
            case 1:
            this.DownloadProgressTextBlock = ((System.Windows.Controls.TextBlock)(target));
            return;
            case 2:
            this.DownloadProgressbar = ((System.Windows.Controls.ProgressBar)(target));
            return;
            case 3:
            this.CefWebBrowser = ((CefSharp.Wpf.ChromiumWebBrowser)(target));
            return;
            case 4:
            this.DownloadNowBtn = ((System.Windows.Controls.Button)(target));
            
            #line 16 "..\..\..\..\UserControls\DownloadDialog.xaml"
            this.DownloadNowBtn.Click += new System.Windows.RoutedEventHandler(this.DownloadNowBtn_Click);
            
            #line default
            #line hidden
            return;
            case 5:
            this.CancelBtn = ((System.Windows.Controls.Button)(target));
            
            #line 17 "..\..\..\..\UserControls\DownloadDialog.xaml"
            this.CancelBtn.Click += new System.Windows.RoutedEventHandler(this.CancelBtn_Click);
            
            #line default
            #line hidden
            return;
            }
            this._contentLoaded = true;
        }
    }
}

