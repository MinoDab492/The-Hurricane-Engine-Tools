﻿using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using System;
using System.IO;
using System.Windows.Controls;
using System.Windows.Media.Imaging;
using System.Windows;
using System.Collections.Generic;
using System.Diagnostics;
using System.Text.RegularExpressions;

namespace UnrealBinaryBuilder.UserControls
{
	public partial class PluginCard : UserControl
	{
		public readonly string PluginPath = null;
		public readonly string DestinationPath = null;
		public readonly string RunUATFile = null;

		private readonly List<string> TargetPlatforms = null;
		private readonly bool IsUsing2019Compiler = false;
		private bool bBuildFinished = false;
		private readonly bool bCanZip = false;
		private readonly bool bZipForMarketplaceZip = false;
		private string TargetZipPath = null;
		private readonly MainWindow mainWindow = null;

		public PluginCard(MainWindow _mainWindow, string InPluginPath, string InDestination, string InEnginePath, bool bUse2019Compiler, List<string> InTargetPlatformsList, bool bZipBuild, string ZipPath, bool bForMarketplace)
		{
			InitializeComponent();
			mainWindow = _mainWindow;
			TargetPlatforms = InTargetPlatformsList;
			IsUsing2019Compiler = bUse2019Compiler;
			CompilerText.Text = IsUsing2019Compiler ? "2019" : "2017";
			PluginPath = InPluginPath;
			DestinationPath = InDestination;
			RunUATFile = Path.Combine(InEnginePath, "Engine", "Build", "BatchFiles", "RunUAT.bat");
			PluginName.Text = Path.GetFileNameWithoutExtension(PluginPath);
			PluginName.ToolTip = PluginPath;
			using (StreamReader reader = File.OpenText(PluginPath))
			{
				JObject o = (JObject)JToken.ReadFrom(new JsonTextReader(reader));
				PluginDescription.Text = o.GetValue("Description").ToString();
				PluginDescription.ToolTip = PluginDescription.Text;
			}

			string PluginIcon = Path.Combine(InPluginPath.Replace(Path.GetFileName(InPluginPath), ""), "Resources", "Icon128.png");
			if (File.Exists(PluginIcon))
			{
				PluginImage.Source = new BitmapImage(new Uri(PluginIcon));
			}
			LoadingCircle.Visibility = Visibility.Collapsed;
			OpenBtn.Visibility = Visibility.Collapsed;
			ZipProgressbar.Visibility = Visibility.Collapsed;

			const string DigitsPattern = @"\d.+";
			Regex DigitsPatternRgx = new Regex(DigitsPattern, RegexOptions.IgnoreCase);
			EngineVersionText.Text = DigitsPatternRgx.Match(InEnginePath).Value;

			bCanZip = bZipBuild;
			TargetZipPath = ZipPath;
			bZipForMarketplaceZip = bForMarketplace;
		}

		public bool IsValid()
		{
			return File.Exists(PluginPath) && Directory.Exists(DestinationPath) && File.Exists(RunUATFile);
		}

		public string GetTargetPlatforms()
		{
			if (TargetPlatforms != null)
			{
				string TargetPlatformsString = "";
				foreach (string s in TargetPlatforms)
				{
					TargetPlatformsString += $"{s}+";
				}

				return $"-TargetPlatforms={TargetPlatformsString.Remove(TargetPlatformsString.Length - 1, 1)}";
			}

			return "";
		}

		public void BuildStarted()
		{
			LoadingCircle.Visibility = Visibility.Visible;
			CancelBtn.Visibility = Visibility.Collapsed;
			OpenBtn.Visibility = Visibility.Collapsed;
		}

		public void PluginFinishedBuild(bool bWasSuccess)
		{
			bBuildFinished = true;
			if (bWasSuccess)
			{
				OpenBtn.Visibility = Visibility.Visible;
				LoadingCircle.Visibility = Visibility.Collapsed;
				CancelBtn.Visibility = Visibility.Collapsed;
				if (bCanZip)
				{
					if (mainWindow.postBuildSettings.DirectoryIsWritable(TargetZipPath) == false)
					{
						TargetZipPath = DestinationPath;
						mainWindow.AddZipLog($"{PluginName.Text} - Zip path was not found or not writable. New save location is {TargetZipPath}", MainWindow.ZipLogInclusionType.FileSkipped);
					}

					ZipProgressbar.Visibility = Visibility.Visible;
					mainWindow.postBuildSettings.SavePluginToZip(this, $"{TargetZipPath}\\{Path.GetFileNameWithoutExtension(PluginPath)}_{EngineVersionText.Text}.zip", bZipForMarketplaceZip);
				}
			}
			else
			{
				CancelBtn.Visibility = Visibility.Visible;
				OpenBtn.Visibility = Visibility.Collapsed;
				LoadingCircle.Visibility = Visibility.Collapsed;
				ZipProgressbar.Visibility = Visibility.Collapsed;
			}
		}

		public bool IsPending()
		{
			return bBuildFinished == false;
		}

		public string GetCompiler()
		{
			return IsUsing2019Compiler ? "-VS2019" : "-VS2017";
		}

		private void CancelBtn_Click(object sender, RoutedEventArgs e)
		{
			MainWindow mainWindow = (MainWindow)Application.Current.MainWindow;
			mainWindow.RemovePluginFromList(this);
		}

		private void OpenBtn_Click(object sender, RoutedEventArgs e)
		{
			_ = Process.Start("explorer.exe", DestinationPath);
		}
	}
}
