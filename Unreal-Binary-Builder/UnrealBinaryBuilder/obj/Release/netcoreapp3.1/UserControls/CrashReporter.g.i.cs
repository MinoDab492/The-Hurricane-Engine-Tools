﻿#pragma checksum "..\..\..\..\UserControls\CrashReporter.xaml" "{ff1816ec-aa5e-4d10-87f7-6f4963833460}" "0FF08F81A1087350D0B873DA66E2F05A2EC7C425"
//------------------------------------------------------------------------------
// <auto-generated>
//     This code was generated by a tool.
//     Runtime Version:4.0.30319.42000
//
//     Changes to this file may cause incorrect behavior and will be lost if
//     the code is regenerated.
// </auto-generated>
//------------------------------------------------------------------------------

using HandyControl.Controls;
using HandyControl.Data;
using HandyControl.Expression.Media;
using HandyControl.Expression.Shapes;
using HandyControl.Interactivity;
using HandyControl.Media.Animation;
using HandyControl.Media.Effects;
using HandyControl.Properties.Langs;
using HandyControl.Themes;
using HandyControl.Tools;
using HandyControl.Tools.Converter;
using HandyControl.Tools.Extension;
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
    /// CrashReporter
    /// </summary>
    public partial class CrashReporter : HandyControl.Controls.GlowWindow, System.Windows.Markup.IComponentConnector {
        
        
        #line 32 "..\..\..\..\UserControls\CrashReporter.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal HandyControl.Controls.TextBox StackTraceText;
        
        #line default
        #line hidden
        
        
        #line 35 "..\..\..\..\UserControls\CrashReporter.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal HandyControl.Controls.TextBox Username;
        
        #line default
        #line hidden
        
        
        #line 38 "..\..\..\..\UserControls\CrashReporter.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal HandyControl.Controls.TextBox Email;
        
        #line default
        #line hidden
        
        
        #line 41 "..\..\..\..\UserControls\CrashReporter.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal HandyControl.Controls.TextBox Comment;
        
        #line default
        #line hidden
        
        
        #line 46 "..\..\..\..\UserControls\CrashReporter.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.Button SubmitBtn;
        
        #line default
        #line hidden
        
        
        #line 47 "..\..\..\..\UserControls\CrashReporter.xaml"
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
            System.Uri resourceLocater = new System.Uri("/UnrealBinaryBuilder;V3.1.6.0;component/usercontrols/crashreporter.xaml", System.UriKind.Relative);
            
            #line 1 "..\..\..\..\UserControls\CrashReporter.xaml"
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
            this.StackTraceText = ((HandyControl.Controls.TextBox)(target));
            return;
            case 2:
            this.Username = ((HandyControl.Controls.TextBox)(target));
            return;
            case 3:
            this.Email = ((HandyControl.Controls.TextBox)(target));
            return;
            case 4:
            this.Comment = ((HandyControl.Controls.TextBox)(target));
            return;
            case 5:
            this.SubmitBtn = ((System.Windows.Controls.Button)(target));
            
            #line 46 "..\..\..\..\UserControls\CrashReporter.xaml"
            this.SubmitBtn.Click += new System.Windows.RoutedEventHandler(this.SubmitBtn_Click);
            
            #line default
            #line hidden
            return;
            case 6:
            this.CancelBtn = ((System.Windows.Controls.Button)(target));
            
            #line 47 "..\..\..\..\UserControls\CrashReporter.xaml"
            this.CancelBtn.Click += new System.Windows.RoutedEventHandler(this.CancelBtn_Click);
            
            #line default
            #line hidden
            return;
            }
            this._contentLoaded = true;
        }
    }
}

