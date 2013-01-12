﻿// Copyright 2012-2013 Alalf <alalf.iQLc_at_gmail.com>
//
// This file is part of SCFF-DirectShow-Filter(SCFF DSF).
//
// SCFF DSF is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// SCFF DSF is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with SCFF DSF.  If not, see <http://www.gnu.org/licenses/>.

/// @file SCFF.GUI/MainWindow.xaml.cs
/// @copydoc SCFF::GUI::MainWindow

namespace SCFF.GUI {

using System.Diagnostics;
using System.IO;
using System.Windows;
using System.Windows.Input;
using Microsoft.Win32;
using Microsoft.Windows.Shell;
using SCFF.Common;
using SCFF.Common.GUI;

/// MainWindowのコードビハインド
public partial class MainWindow
    : Window, IBindingProfile, IBindingOptions, IBindingRuntimeOptions {
  //===================================================================
  // コンストラクタ/Dispose/デストラクタ
  //===================================================================

  /// コンストラクタ
  public MainWindow() {
    this.InitializeComponent();

    this.OnOptionsChanged();
    this.OnRuntimeOptionsChanged();
    this.OnEntireProfileChanged();

    // 必要な機能の実行
    this.SetAero();
    this.SetCompactView();
  }

  //===================================================================
  // イベントハンドラ
  //===================================================================

  /// アプリケーション終了時に発生するClosingイベントハンドラ
  protected override void OnClosing(System.ComponentModel.CancelEventArgs e) {
    base.OnClosing(e);

    this.SaveTemporaryOptions();
  }

  /// Deactivated
  /// @param e 使用しない
  protected override void OnDeactivated(System.EventArgs e) {
    base.OnDeactivated(e);

    /// @todo(me) スクリーンキャプチャをの更新頻度を下げる
    ///           App.RuntimeOptionsに該当するデータを保存しておく感じかな？
    // Debug.WriteLine("Deactivated", "MainWindow");
  }

  /// Activated
  /// @param e 使用しない
  protected override void OnActivated(System.EventArgs e) {
    base.OnActivated(e);

    /// @todo(me) スクリーンキャプチャを更新頻度を元に戻す
    ///           App.RuntimeOptionsに該当するデータを保存しておく感じかな？
    // Debug.WriteLine("Activated", "MainWindow");
  }

  //-------------------------------------------------------------------
  // *Changed/Checked/Unchecked以外
  //-------------------------------------------------------------------

  //-------------------------------------------------------------------
  // Checked/Unchecked
  //-------------------------------------------------------------------

  //-------------------------------------------------------------------
  // *Changed/Collapsed/Expanded
  //-------------------------------------------------------------------

  /// AreaExpander: Collapsed
  private void AreaExpander_Collapsed(object sender, RoutedEventArgs e) {
    if (!this.CanChangeOptions) return;
    App.Options.AreaIsExpanded = false;
  }
  /// AreaExpander: Expanded
  private void AreaExpander_Expanded(object sender, RoutedEventArgs e) {
    if (!this.CanChangeOptions) return;
    App.Options.AreaIsExpanded = true;
  }
  /// OptionsExpander: Collapsed
  private void OptionsExpander_Collapsed(object sender, RoutedEventArgs e) {
    if (!this.CanChangeOptions) return;
    App.Options.OptionsIsExpanded = false;
  }
  /// OptionsExpander: Expanded
  private void OptionsExpander_Expanded(object sender, RoutedEventArgs e) {
    if (!this.CanChangeOptions) return;
    App.Options.OptionsIsExpanded = true;
  }
  /// ResizeMethodExpander: Collapsed
  private void ResizeMethodExpander_Collapsed(object sender, RoutedEventArgs e) {
    if (!this.CanChangeOptions) return;
    App.Options.ResizeMethodIsExpanded = false;
  }
  /// ResizeMethodExpander: Expanded
  private void ResizeMethodExpander_Expanded(object sender, RoutedEventArgs e) {
    if (!this.CanChangeOptions) return;
    App.Options.ResizeMethodIsExpanded = true;
  }
  /// LayoutExpander: Collapsed
  private void LayoutExpander_Collapsed(object sender, RoutedEventArgs e) {
    if (!this.CanChangeOptions) return;
    App.Options.LayoutIsExpanded = false;

    UpdateCommands.UpdateLayoutEditByOptions.Execute(null, null);
  }
  /// LayoutExpander: Expanded
  private void LayoutExpander_Expanded(object sender, RoutedEventArgs e) {
    if (!this.CanChangeOptions) return;
    App.Options.LayoutIsExpanded = true;

    UpdateCommands.UpdateLayoutEditByOptions.Execute(null, null);
  }

  //===================================================================
  // IBindingOptionsの実装
  //===================================================================

  /// @copydoc Common::GUI::IBindingOptions::CanChangeOptions
  public bool CanChangeOptions { get; private set; }
  /// @copydoc Common::GUI::IBindingOptions::OnOptionsChanged
  public void OnOptionsChanged() {
    this.CanChangeOptions = false;

    // Temporary
    this.Left         = App.Options.TmpMainWindowLeft;
    this.Top          = App.Options.TmpMainWindowTop;
    this.Width        = App.Options.TmpMainWindowWidth;
    this.Height       = App.Options.TmpMainWindowHeight;
    this.WindowState  = (System.Windows.WindowState)App.Options.TmpMainWindowState;
    
    // MainWindow.Controls
    this.AreaExpander.IsExpanded          = App.Options.AreaIsExpanded;
    this.OptionsExpander.IsExpanded       = App.Options.OptionsIsExpanded;
    this.ResizeMethodExpander.IsExpanded  = App.Options.ResizeMethodIsExpanded;
    this.LayoutExpander.IsExpanded        = App.Options.LayoutIsExpanded;

    // UserControls
    this.Apply.OnOptionsChanged();
    this.LayoutToolbar.OnOptionsChanged();
    this.LayoutEdit.OnOptionsChanged();
    this.MainMenu.OnOptionsChanged();

    this.CanChangeOptions = true;
  }

  /// UIから設定にデータを保存
  private void SaveTemporaryOptions() {
    // Tmp接頭辞のプロパティだけはここで更新する必要がある
    var isNormal = this.WindowState == System.Windows.WindowState.Normal;
    App.Options.TmpMainWindowLeft = isNormal ? this.Left : this.RestoreBounds.Left;
    App.Options.TmpMainWindowTop = isNormal ? this.Top : this.RestoreBounds.Top;
    App.Options.TmpMainWindowWidth = isNormal ? this.Width : this.RestoreBounds.Width;
    App.Options.TmpMainWindowHeight = isNormal ? this.Height : this.RestoreBounds.Height;
    App.Options.TmpMainWindowState = (SCFF.Common.WindowState)this.WindowState;
  }

  //===================================================================
  // IBindingRuntimeOptionsの実装
  //===================================================================

  /// @copydoc Common::GUI::IBindingRuntimeOptions::CanChangeRuntimeOptions
  public bool CanChangeRuntimeOptions { get; private set; }
  /// @copydoc Common::GUI::IBindingRuntimeOptions::OnRuntimeOptionsChanged
  public void OnRuntimeOptionsChanged() {
    this.CanChangeRuntimeOptions = false;
    /// @todo System.Reflection.Assembly.GetExecutingAssembly().GetName().Versionを使うか？
    ///       しかしどう見てもこれ実行時に決まる値で気持ち悪いな・・・
    var commonTitle = "SCFF DirectShow Filter Ver.0.1.7";
    if (App.RuntimeOptions.ProfilePath != string.Empty) {
      var profileName = Path.GetFileNameWithoutExtension(App.RuntimeOptions.ProfilePath);
      this.WindowTitle.Content = string.Format("{0} - {1}", profileName, commonTitle);
    } else {
      this.WindowTitle.Content = commonTitle;
    }

    this.LayoutEdit.OnRuntimeOptionsChanged();
    this.LayoutParameter.OnRuntimeOptionsChanged();
    this.CanChangeRuntimeOptions = true;
  }

  //===================================================================
  // IBindingProfileの実装
  //===================================================================

  /// @copydoc Common::GUI::IBindingProfile::CanChangeProfile
  public bool CanChangeProfile { get; private set; }

  /// @copydoc Common::GUI::IBindingProfile::OnCurrentProfileChanged
  public void OnCurrentProfileChanged() {
    this.CanChangeProfile = false;
    this.TargetWindow.OnCurrentProfileChanged();
    this.Area.OnCurrentProfileChanged();
    this.Options.OnCurrentProfileChanged();
    this.ResizeMethod.OnCurrentProfileChanged();
    this.LayoutParameter.OnCurrentProfileChanged();
    this.LayoutTab.OnCurrentProfileChanged();
    this.LayoutEdit.OnCurrentProfileChanged();
    this.CanChangeProfile = true;
  }

  /// @copydoc Common::GUI::IBindingProfile::OnEntireProfileChanged
  public void OnEntireProfileChanged() {
    this.CanChangeProfile = false;
    this.TargetWindow.OnEntireProfileChanged();
    this.Area.OnEntireProfileChanged();
    this.Options.OnEntireProfileChanged();
    this.ResizeMethod.OnEntireProfileChanged();
    this.LayoutParameter.OnEntireProfileChanged();
    this.LayoutTab.OnEntireProfileChanged();
    this.LayoutEdit.OnEntireProfileChanged();
    this.CanChangeProfile = true;
  }

  //===================================================================
  // コマンドイベントハンドラ
  //===================================================================

  //-------------------------------------------------------------------
  // ApplicationCommands
  //-------------------------------------------------------------------

  /// New
  /// @param sender 使用しない
  /// @param e 使用しない
  private void New_Executed(object sender, ExecutedRoutedEventArgs e) {
    var result = MessageBox.Show("Do you want to save changes?",
                                 "SCFF.GUI",
                                 MessageBoxButton.YesNoCancel,
                                 MessageBoxImage.Warning,
                                 MessageBoxResult.Yes);
    switch (result) {
      case MessageBoxResult.No: {
        App.Profile.RestoreDefault();
        this.OnEntireProfileChanged();
        break;
      }
      case MessageBoxResult.Yes: {
        var save = new SaveFileDialog();
        save.Title = "SCFF.GUI";
        save.Filter = "SCFF.GUI Profile|*.SCFF.GUI.profile";
        var saveResult = save.ShowDialog();
        if (saveResult.HasValue && (bool)saveResult) {
          /// @todo(me) 実装
          MessageBox.Show(save.FileName);
          App.Options.AddRecentProfile(save.FileName);
          this.MainMenu.OnOptionsChanged();

          App.Profile.RestoreDefault();
          this.OnEntireProfileChanged();
        }
        break;
      }
    }
  }

  /// Open
  /// @param sender 使用しない
  /// @param e 使用しない
  private void Open_Executed(object sender, ExecutedRoutedEventArgs e) {
    /// @todo(me) Newと似たコードが必要だがかなりめんどくさい。あとでかく
  }

  /// Save
  /// @param sender 使用しない
  /// @param e 使用しない
  private void Save_Executed(object sender, ExecutedRoutedEventArgs e) {
    /// @todo(me) すでに保存されていない場合はダイアログをだす
    var save = new SaveFileDialog();
    save.Title = "SCFF.GUI";
    save.Filter = "SCFF.GUI Profile|*.SCFF.GUI.profile";
    var saveResult = save.ShowDialog();
    if (saveResult.HasValue && (bool)saveResult) {
      /// @todo(me) 実装
      MessageBox.Show(save.FileName);
      App.Options.AddRecentProfile(save.FileName);
      this.MainMenu.OnOptionsChanged();
    }
  }

  /// SaveAs
  /// @param sender 使用しない
  /// @param e 使用しない
  private void SaveAs_Executed(object sender, ExecutedRoutedEventArgs e) {
    var save = new SaveFileDialog();
    save.Title = "SCFF.GUI";
    save.Filter = "SCFF.GUI Profile|*.SCFF.GUI.profile";
    var saveResult = save.ShowDialog();
    if (saveResult.HasValue && (bool)saveResult) {
      /// @todo(me) 実装
      MessageBox.Show(save.FileName);
      App.Options.AddRecentProfile(save.FileName);
      this.MainMenu.OnOptionsChanged();
    }
  }

  //-------------------------------------------------------------------
  // Windows.Shell.SystemCommands
  //-------------------------------------------------------------------
  
  /// CloseWindow
	private void CloseWindow_Executed(object sender, ExecutedRoutedEventArgs e) {
		SystemCommands.CloseWindow(this);
	}
  /// MaximizeWindow
	private void MaximizeWindow_Executed(object sender, ExecutedRoutedEventArgs e) {
		SystemCommands.MaximizeWindow(this);
	}
  /// MinimizeWindow
	private void MinimizeWindow_Executed(object sender, ExecutedRoutedEventArgs e) {
		SystemCommands.MinimizeWindow(this);
	}
  /// RestoreWindow
	private void RestoreWindow_Executed(object sender, ExecutedRoutedEventArgs e) {
		SystemCommands.RestoreWindow(this);
	}

  //-------------------------------------------------------------------
  // SCFF.GUI.UpdateCommands
  //-------------------------------------------------------------------

  /// @copydoc SCFF::GUI::UpdateCommands::UpdateMainWindowByEntireProfile
  /// @param sender 使用しない
  /// @param e 使用しない
  private void UpdateMainWindowByEntireProfile_Executed(object sender, ExecutedRoutedEventArgs e) {
    this.OnEntireProfileChanged();
  }
  /// @copydoc SCFF::GUI::UpdateCommands::UpdateLayoutEditByEntireProfile
  /// @param sender 使用しない
  /// @param e 使用しない
  private void UpdateLayoutEditByEntireProfile_Executed(object sender, ExecutedRoutedEventArgs e) {
    this.LayoutEdit.OnEntireProfileChanged();
  }
  /// @copydoc SCFF::GUI::UpdateCommands::UpdateLayoutEditByCurrentProfile
  /// @param sender 使用しない
  /// @param e 使用しない
  private void UpdateLayoutEditByCurrentProfile_Executed(object sender, ExecutedRoutedEventArgs e) {
    this.LayoutEdit.OnCurrentProfileChanged();
  }
  /// @copydoc SCFF::GUI::UpdateCommands::UpdateTargetWindowByCurrentProfile
  /// @param sender 使用しない
  /// @param e 使用しない
  private void UpdateTargetWindowByCurrentProfile_Executed(object sender, ExecutedRoutedEventArgs e) {
    this.TargetWindow.OnCurrentProfileChanged();
    this.Area.OnCurrentProfileChanged();
    this.LayoutEdit.OnCurrentProfileChanged();
    this.LayoutParameter.OnCurrentProfileChanged();
  }
  /// @copydoc SCFF::GUI::UpdateCommands::UpdateLayoutParameterByCurrentProfile
  /// @param sender 使用しない
  /// @param e 使用しない
  private void UpdateLayoutParameterByCurrentProfile_Executed(object sender, ExecutedRoutedEventArgs e) {
    this.LayoutParameter.OnCurrentProfileChanged();
  }
  /// @copydoc SCFF::GUI::UpdateCommands::UpdateLayoutEditByOptions
  /// @param sender 使用しない
  /// @param e 使用しない
  private void UpdateLayoutEditByOptions_Executed(object sender, ExecutedRoutedEventArgs e) {
    this.LayoutEdit.OnOptionsChanged();
  }
  /// @copydoc SCFF::GUI::UpdateCommands::UpdateMainWindowByRuntimeOptions
  /// @param sender 使用しない
  /// @param e 使用しない
  private void UpdateMainWindowByRuntimeOptions_Executed(object sender, ExecutedRoutedEventArgs e) {
    this.OnRuntimeOptionsChanged();
  }

  //-------------------------------------------------------------------
  // SCFF.GUI.Commands
  //-------------------------------------------------------------------

  /// AeroをON/OFF
  private void SetAero() {
    // @todo(me) 実装
  }

  /// AeroのON/OFFが可能か
  private bool CanUseAero() {
    // @todo(me) 実装
    return true;
  }

  /// @copydoc SetAero
  /// @param sender 使用しない
  /// @param e 使用しない
  private void SetAero_Executed(object sender, ExecutedRoutedEventArgs e) {
    Debug.WriteLine("Execute", "[Command] SetAero");
    this.SetAero();
  }

  /// @copydoc CanUseAero
  /// @param sender 使用しない
  /// @param[out] e 実行可能か(CanExecute)を設定可能
  private void SetAero_CanExecute(object sender, CanExecuteRoutedEventArgs e) {
    e.CanExecute = this.CanUseAero();
  }

  //-------------------------------------------------------------------

  /// コンパクト表示切替
  private void SetCompactView() {
    if (App.Options.CompactView) {
      this.OptionsExpander.Visibility = Visibility.Collapsed;
      this.ResizeMethodExpander.Visibility = Visibility.Collapsed;
      this.LayoutExpander.IsExpanded = false;
      this.Width = Constants.CompactMainWindowWidth;
      this.Height = Constants.CompactMainWindowHeight;
    } else {
      this.OptionsExpander.Visibility = Visibility.Visible;
      this.ResizeMethodExpander.Visibility = Visibility.Visible;
    }
  }

  /// @copydoc SetCompactView
  /// @param sender 使用しない
  /// @param e 使用しない
  private void SetCompactView_Executed(object sender, ExecutedRoutedEventArgs e) {
    Debug.WriteLine("Execute", "[Command] SetCompactView");
    this.SetCompactView();
  }
}
}   // namespace SCFF.GUI
