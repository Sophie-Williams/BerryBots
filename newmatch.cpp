/*
  Copyright (C) 2012 - Voidious

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include <iostream>
#include <wx/wx.h>
#include <wx/artprov.h>
#include <wx/iconbndl.h>
#include <wx/mimetype.h>
#include "bbwx.h"
#include "basedir.h"
#include "filemanager.h"
#include "newmatch.h"

NewMatchDialog::NewMatchDialog(NewMatchListener *listener,
    MenuBarMaker *menuBarMaker) : wxFrame(NULL, NEW_MATCH_ID, "New Match",
        wxPoint(50, 50), wxDefaultSize,
        wxDEFAULT_FRAME_STYLE & ~ (wxRESIZE_BORDER | wxMAXIMIZE_BOX)) {
  listener_ = listener;
  menuBarMaker_ = menuBarMaker;

#ifdef __WINDOWS__
  SetIcon(wxIcon("berrybots.ico", wxBITMAP_TYPE_ICO));

  // The 8-9 point default font size in Windows is much smaller than Mac/Linux.
  wxFont windowFont = GetFont();
  if (windowFont.GetPointSize() <= 9) {
    SetFont(windowFont.Larger());
  }
#elif defined(__WXGTK__)
  SetIcon(wxIcon("icon_128x128.png", wxBITMAP_TYPE_PNG));
#endif

  mainPanel_ = new wxPanel(this);
  mainSizer_ = new wxBoxSizer(wxHORIZONTAL);
  mainSizer_->Add(mainPanel_);
  borderSizer_ = new wxBoxSizer(wxHORIZONTAL);
  wxFlexGridSizer *gridSizer = new wxFlexGridSizer(2, 5, 5);
  wxBoxSizer *stageLabelSizer = new wxBoxSizer(wxHORIZONTAL);
  stageLabel_ = new wxStaticText(mainPanel_, wxID_ANY, "Stage:");
  stageLabelSizer->Add(stageLabel_, 0, wxALIGN_LEFT);
  stageLabelSizer->AddSpacer(27);
  previewLabel_ = new wxStaticText(mainPanel_, wxID_ANY, "<space> to preview");
  stageLabelSizer->Add(previewLabel_);
  wxBoxSizer *stageSizer = new wxBoxSizer(wxVERTICAL);
  stageSizer->Add(stageLabelSizer);
  stageSizer->AddSpacer(3);
  stageSelect_ = new wxListBox(mainPanel_, wxID_ANY, wxDefaultPosition,
                               wxSize(275, 225), 0, NULL, wxLB_SORT);
  stageSizer->Add(stageSelect_, 0, wxALIGN_LEFT);
  gridSizer->Add(stageSizer, 0, wxALIGN_LEFT);

  wxBoxSizer *dirsSizer = new wxStaticBoxSizer(wxVERTICAL, mainPanel_);
  wxBoxSizer *stagesBaseDirSizer = new wxBoxSizer(wxHORIZONTAL);
  stagesBaseDirLabel_ = new wxStaticText(mainPanel_, wxID_ANY, "Stages:");
  stagesBaseDirValueLabel_ = new wxStaticText(mainPanel_, wxID_ANY,
                                              wxEmptyString);
  stagesBaseDirValueLabel_->SetFont(GetFont().Smaller());
  stagesBaseDirSizer->Add(stagesBaseDirLabel_);
  stagesBaseDirSizer->AddSpacer(5);
  stagesBaseDirSizer->Add(stagesBaseDirValueLabel_, 0 ,wxALIGN_BOTTOM);
  wxBoxSizer *shipsBaseDirSizer = new wxBoxSizer(wxHORIZONTAL);
  shipsBaseDirLabel_ = new wxStaticText(mainPanel_, wxID_ANY, "Ships:");
  shipsBaseDirValueLabel_ = new wxStaticText(mainPanel_, wxID_ANY,
                                             wxEmptyString);
  shipsBaseDirValueLabel_->SetFont(GetFont().Smaller());
  shipsBaseDirSizer->Add(shipsBaseDirLabel_);
  shipsBaseDirSizer->AddSpacer(5);
  shipsBaseDirSizer->Add(shipsBaseDirValueLabel_, 0 ,wxALIGN_BOTTOM);
  updateBaseDirLabels();
  dirsSizer->Add(stagesBaseDirSizer);
#if defined(__WXOSX__) || defined(__LINUX__) || defined(__WINDOWS__)
  browseStagesButton_ = new wxButton(mainPanel_, wxID_ANY, "Browse");
#ifndef __WINDOWS__
  // Bizarrely, it's impossible to add any padding between the bitmap and the
  // edge of the button on Windows. It's ugly enough to just not set a bitmap.
  browseStagesButton_->SetBitmap(wxArtProvider::GetBitmap(wxART_FOLDER_OPEN));
#endif
  dirsSizer->AddSpacer(3);
  dirsSizer->Add(browseStagesButton_);
#endif

  dirsSizer->AddSpacer(12);
  dirsSizer->Add(shipsBaseDirSizer);
#if defined(__WXOSX__) || defined(__LINUX__) || defined(__WINDOWS__)
  browseShipsButton_ = new wxButton(mainPanel_, wxID_ANY, "Browse");
#ifndef __WINDOWS__
  // Don't set impossible to align bitmap on Windows.
  browseShipsButton_->SetBitmap(wxArtProvider::GetBitmap(wxART_FOLDER_OPEN));
#endif
  dirsSizer->AddSpacer(3);
  dirsSizer->Add(browseShipsButton_);
#endif

  dirsSizer->AddStretchSpacer(1);
  wxBoxSizer *moreButtonsSizer = new wxBoxSizer(wxHORIZONTAL);
  browseApidocsButton_ = new wxButton(mainPanel_, wxID_ANY, "&API Docs");
#ifndef __WINDOWS__
  // Don't set impossible to align bitmap on Windows.
  browseApidocsButton_->SetBitmap(wxArtProvider::GetBitmap(wxART_FILE_OPEN));
#endif
  moreButtonsSizer->Add(browseApidocsButton_);

#ifdef __WXOSX__
  // Using cwd as base dir on other platforms, so only support changing base dir
  // on Mac OS X for now.
  folderButton_ = new wxButton(mainPanel_, wxID_ANY, "&Base Dir ");
  folderButton_->SetBitmap(wxArtProvider::GetBitmap(wxART_FOLDER_OPEN));
  moreButtonsSizer->AddSpacer(12);
  moreButtonsSizer->Add(folderButton_);
  Connect(folderButton_->GetId(), wxEVT_COMMAND_BUTTON_CLICKED,
          wxCommandEventHandler(NewMatchDialog::onChangeBaseDir));
#endif

  dirsSizer->AddStretchSpacer(1);
  dirsSizer->Add(moreButtonsSizer, 0, wxALIGN_BOTTOM);
  dirsSizer->AddSpacer(4);
  gridSizer->Add(dirsSizer, 0, wxEXPAND | wxLEFT, 8);

  shipsLabel_ = new wxStaticText(mainPanel_, wxID_ANY, "Ships:");
  shipsSelect_ = new wxListBox(mainPanel_, wxID_ANY, wxDefaultPosition,
                               wxSize(275, 225), 0, NULL,
                               wxLB_EXTENDED | wxLB_SORT);
  wxBoxSizer *shipsSizer = new wxBoxSizer(wxVERTICAL);
  shipsSizer->Add(shipsLabel_, 0, wxALIGN_LEFT);
  shipsSizer->AddSpacer(3);
  shipsSizer->Add(shipsSelect_, 0, wxALIGN_LEFT);
  gridSizer->Add(shipsSizer, 0, wxALIGN_LEFT);

  wxBoxSizer *buttonsLoadedShipsSizer = new wxBoxSizer(wxHORIZONTAL);
  addArrow_ = new wxButton(mainPanel_, wxID_ANY, ">>", wxDefaultPosition,
                           wxDefaultSize);
  removeArrow_ = new wxButton(mainPanel_, wxID_ANY, "<<", wxDefaultPosition,
                              wxDefaultSize);
  clearButton_ = new wxButton(mainPanel_, wxID_ANY, "C&lear", wxDefaultPosition,
                              wxDefaultSize);

  wxBoxSizer *shipButtonsSizer = new wxBoxSizer(wxVERTICAL);
  shipButtonsSizer->AddStretchSpacer(1);
  shipButtonsSizer->Add(addArrow_, 0, wxALIGN_CENTER);
  shipButtonsSizer->AddSpacer(5);
  shipButtonsSizer->Add(removeArrow_, 0, wxALIGN_CENTER);
  shipButtonsSizer->AddSpacer(5);
  shipButtonsSizer->Add(clearButton_, 0, wxALIGN_CENTER);
  shipButtonsSizer->AddStretchSpacer(1);
#ifdef __WXOSX__
  keyboardLabel_ = new wxStaticText(mainPanel_, wxID_ANY,
                                    "\u2318 hotkeys");
#else
  keyboardLabel_ = new wxStaticText(mainPanel_, wxID_ANY,
                                    "ALT hotkeys");
#endif
  shipButtonsSizer->Add(keyboardLabel_, 0, wxALIGN_CENTER | wxALIGN_BOTTOM);
  buttonsLoadedShipsSizer->Add(shipButtonsSizer, 0, wxALIGN_CENTER | wxEXPAND);

  loadedShipsSelect_ = new wxListBox(mainPanel_, wxID_ANY, wxDefaultPosition,
                                     wxSize(275, 225), 0, NULL, wxLB_EXTENDED);
  buttonsLoadedShipsSizer->AddSpacer(5);
  buttonsLoadedShipsSizer->Add(loadedShipsSelect_, 0,
                              wxALIGN_BOTTOM | wxALIGN_RIGHT);
  gridSizer->Add(buttonsLoadedShipsSizer, 0, wxALIGN_BOTTOM);

  refreshButton_ = new wxButton(mainPanel_, wxID_REFRESH, "    &Refresh    ");
  gridSizer->Add(refreshButton_, 0, wxALIGN_LEFT);

  startButton_ = new wxButton(mainPanel_, wxID_ANY, "    Start &Match!    ",
                              wxDefaultPosition, wxDefaultSize);
  browseApidocsButton_->MoveAfterInTabOrder(browseShipsButton_);
  gridSizer->Add(startButton_, 0, wxALIGN_RIGHT);
  borderSizer_->Add(gridSizer, 0, wxALL, 12);
  mainPanel_->SetSizerAndFit(borderSizer_);
  SetSizerAndFit(mainSizer_);

  numStages_ = numShips_ = numLoadedShips_ = 0;
  menusInitialized_ = false;
  validateButtons();

  Connect(NEW_MATCH_ID, wxEVT_ACTIVATE,
          wxActivateEventHandler(NewMatchDialog::onActivate));
  Connect(NEW_MATCH_ID, wxEVT_CLOSE_WINDOW,
          wxCommandEventHandler(NewMatchDialog::onClose));
  Connect(addArrow_->GetId(), wxEVT_COMMAND_BUTTON_CLICKED,
          wxCommandEventHandler(NewMatchDialog::onAddShips));
  Connect(removeArrow_->GetId(), wxEVT_COMMAND_BUTTON_CLICKED,
          wxCommandEventHandler(NewMatchDialog::onRemoveShips));
  Connect(clearButton_->GetId(), wxEVT_COMMAND_BUTTON_CLICKED,
          wxCommandEventHandler(NewMatchDialog::onClearLoadedShips));
  Connect(startButton_->GetId(), wxEVT_COMMAND_BUTTON_CLICKED,
          wxCommandEventHandler(NewMatchDialog::onStartMatch));
  Connect(shipsSelect_->GetId(), wxEVT_COMMAND_LISTBOX_DOUBLECLICKED,
          wxCommandEventHandler(NewMatchDialog::onAddShips));
  Connect(loadedShipsSelect_->GetId(), wxEVT_COMMAND_LISTBOX_DOUBLECLICKED,
          wxCommandEventHandler(NewMatchDialog::onRemoveShips));
  Connect(refreshButton_->GetId(), wxEVT_COMMAND_BUTTON_CLICKED,
          wxCommandEventHandler(NewMatchDialog::onRefreshFiles));
  Connect(stageSelect_->GetId(), wxEVT_UPDATE_UI,
          wxUpdateUIEventHandler(NewMatchDialog::onSelectStage));
  Connect(shipsSelect_->GetId(), wxEVT_UPDATE_UI,
          wxUpdateUIEventHandler(NewMatchDialog::onSelectShip));
  Connect(loadedShipsSelect_->GetId(), wxEVT_UPDATE_UI,
          wxUpdateUIEventHandler(NewMatchDialog::onSelectLoadedShip));
  Connect(browseApidocsButton_->GetId(), wxEVT_COMMAND_BUTTON_CLICKED,
          wxCommandEventHandler(NewMatchDialog::onBrowseApidocs));

#if defined(__WXOSX__) || defined(__LINUX__) || defined(__WINDOWS__)
  browseStagesButton_->MoveAfterInTabOrder(startButton_);
  browseShipsButton_->MoveAfterInTabOrder(browseStagesButton_);
  browseApidocsButton_->MoveAfterInTabOrder(browseShipsButton_);
  Connect(browseStagesButton_->GetId(), wxEVT_COMMAND_BUTTON_CLICKED,
          wxCommandEventHandler(NewMatchDialog::onBrowseStages));
  Connect(browseShipsButton_->GetId(), wxEVT_COMMAND_BUTTON_CLICKED,
          wxCommandEventHandler(NewMatchDialog::onBrowseShips));
#endif

#ifdef __WXOSX__
  folderButton_->MoveAfterInTabOrder(browseShipsButton_);
  browseApidocsButton_->MoveAfterInTabOrder(folderButton_);
#endif

  eventFilter_ = new NewMatchEventFilter(this);
  this->GetEventHandler()->AddFilter(eventFilter_);
}

NewMatchDialog::~NewMatchDialog() {
  this->GetEventHandler()->RemoveFilter(eventFilter_);
  delete eventFilter_;
  delete stageLabel_;
  delete stageSelect_;
  delete shipsLabel_;
  delete shipsSelect_;
  delete addArrow_;
  delete removeArrow_;
  delete clearButton_;
  delete loadedShipsSelect_;
  delete startButton_;
  delete refreshButton_;
  delete stagesBaseDirLabel_;
  delete stagesBaseDirValueLabel_;
  delete shipsBaseDirLabel_;
  delete shipsBaseDirValueLabel_;
  delete keyboardLabel_;
  delete browseApidocsButton_;

#if defined(__WXOSX__) || defined(__LINUX__) || defined(__WINDOWS__)
  delete browseStagesButton_;
  delete browseShipsButton_;
#endif

#ifdef __WXOSX__
  delete folderButton_;
#endif

  delete mainPanel_;
}

void NewMatchDialog::clearStages() {
  stageSelect_->Clear();
  numStages_ = 0;
}

void NewMatchDialog::addStage(char *stage) {
  stageSelect_->Append(wxString(stage));
  numStages_++;
  if (stageSelect_->GetCount() > 0) {
    stageSelect_->SetFirstItem(0);
  }
}

void NewMatchDialog::clearShips() {
  shipsSelect_->Clear();
  numShips_ = 0;
}

void NewMatchDialog::addShip(char *ship) {
  shipsSelect_->Append(wxString(ship));
  numShips_++;
  if (shipsSelect_->GetCount() > 0) {
    shipsSelect_->SetFirstItem(0);
  }
}

void NewMatchDialog::onActivate(wxActivateEvent &event) {
  if (event.GetActive()) {
    listener_->onActive();
  }
  if (!menusInitialized_) {
    this->SetMenuBar(menuBarMaker_->getNewMenuBar());
    menusInitialized_ = true;
  }
  SetSizerAndFit(mainSizer_);
}

void NewMatchDialog::onClose(wxCommandEvent &event) {
  listener_->onClose();
}

void NewMatchDialog::onAddShips(wxCommandEvent &event) {
  addSelectedShips();
}

void NewMatchDialog::addSelectedShips() {
  wxArrayInt selectedShips;
  shipsSelect_->GetSelections(selectedShips);
  wxArrayInt::const_iterator first = selectedShips.begin();
  wxArrayInt::const_iterator last = selectedShips.end();
  while (first != last) {
    int shipIndex = *first++;
    loadedShipsSelect_->Insert(shipsSelect_->GetString(shipIndex),
                               numLoadedShips_++);
  }
  validateButtons();
}

void NewMatchDialog::onRemoveShips(wxCommandEvent &event) {
  removeSelectedLoadedShips();
}

void NewMatchDialog::removeSelectedLoadedShips() {
  wxArrayInt selectedShips;
  loadedShipsSelect_->GetSelections(selectedShips);
  wxArrayInt::const_iterator first = selectedShips.begin();
  wxArrayInt::const_iterator last = selectedShips.end();
  int removed = 0;
  while (first != last) {
    int shipIndex = *first++;
    loadedShipsSelect_->Delete(shipIndex - (removed++));
    numLoadedShips_--;
  }
  validateButtons();
}

void NewMatchDialog::removeStaleLoadedShips() {
  if (numLoadedShips_ > 0) {
    for (int x = 0; x < numLoadedShips_; x++) {
      wxString loadedShip = loadedShipsSelect_->GetString(x);
      if (shipsSelect_->wxItemContainerImmutable::FindString(loadedShip)
              == wxNOT_FOUND) {
        loadedShipsSelect_->Delete(x);
        x--;
        numLoadedShips_--;
      }
    }
  }
  validateButtons();
}

void NewMatchDialog::onClearLoadedShips(wxCommandEvent &event) {
  clearLoadedShips();
}

void NewMatchDialog::clearLoadedShips() {
  loadedShipsSelect_->Clear();
  numLoadedShips_ = 0;
  validateButtons();
}

void NewMatchDialog::onStartMatch(wxCommandEvent &event) {
  startMatch();
}

void NewMatchDialog::startMatch() {
  if (listener_ != 0) {
    wxArrayInt selectedStageIndex;
    stageSelect_->GetSelections(selectedStageIndex);
    if (numLoadedShips_ > 0 && selectedStageIndex.Count() > 0) {
      int numStartShips = numLoadedShips_;
      char** ships = new char*[numStartShips];
      for (int x = 0; x < numStartShips; x++) {
        wxString loadedShip = loadedShipsSelect_->GetString(x);
        char *ship = new char[loadedShip.length() + 1];
#ifdef __WINDOWS__
        strcpy(ship, loadedShip.c_str());
#else
        strcpy(ship, loadedShip.fn_str());
#endif
        ships[x] = ship;
      }
      
      wxString selectedStage =
          stageSelect_->GetString(*(selectedStageIndex.begin()));
      char *stage = new char[selectedStage.length() + 1];
#ifdef __WINDOWS__
      strcpy(stage, selectedStage.c_str());
#else
      strcpy(stage, selectedStage.fn_str());
#endif
      
      listener_->startMatch(stage, ships, numStartShips);
      
      for (int x = 0; x < numStartShips; x++) {
        delete ships[x];
      }
      delete ships;
      delete stage;
    }
  }
}

void NewMatchDialog::onRefreshFiles(wxCommandEvent &event) {
  refreshFiles();
}

void NewMatchDialog::refreshFiles() {
  // TODO: reselect previously selected ships and stage
  listener_->refreshFiles();
  validateButtons();
}

void NewMatchDialog::onBrowseStages(wxCommandEvent &event) {
  browseDirectory(getStagesDir().c_str());
}

void NewMatchDialog::onBrowseShips(wxCommandEvent &event) {
  browseDirectory(getShipsDir().c_str());
}

void NewMatchDialog::onBrowseApidocs(wxCommandEvent &event) {
  browseApidocs();
}

void NewMatchDialog::browseApidocs() {
  openHtmlFile(getApidocPath().c_str());
}

void NewMatchDialog::browseDirectory(const char *dir) {
  if (!FileManager::fileExists(dir)) {
    std::string fileNotFoundString("Directory not found: ");
    fileNotFoundString.append(dir);
    wxMessageDialog cantBrowseMessage(this, "Directory not found",
                                      fileNotFoundString, wxOK);
    cantBrowseMessage.ShowModal();
  } else {
#if defined(__WXOSX__)
    ::wxExecute(wxString::Format("open \"%s\"", dir), wxEXEC_ASYNC, NULL);
#elif defined(__LINUX__)
    ::wxExecute(wxString::Format("xdg-open \"%s\"", dir), wxEXEC_ASYNC, NULL);
#elif defined(__WINDOWS__)
    ::wxExecute(wxString::Format("explorer \"%s\"", dir), wxEXEC_ASYNC, NULL);
#else
    wxMessageDialog cantBrowseMessage(this, "Couldn't browse directory",
        "Sorry, don't know how to open/browse files on your platform.", wxOK);
    cantBrowseMessage.ShowModal();
#endif
  }
}

void NewMatchDialog::openHtmlFile(const char *file) {
  if (!FileManager::fileExists(file)) {
    std::string fileNotFoundString("File not found: ");
    fileNotFoundString.append(file);
    wxMessageDialog cantBrowseMessage(this, "File not found",
                                      fileNotFoundString, wxOK);
    cantBrowseMessage.ShowModal();
  } else {
    // On Mac OS X, wxFileType::GetOpenCommand always returns Safari instead of
    // the default browser. And what's worse, Safari doesn't load the CSS
    // properly when we open it that way. But we can trust the 'open' command.
#if defined(__WXOSX__)
    ::wxExecute(wxString::Format("open \"%s\"", file), wxEXEC_ASYNC, NULL);
#else
    wxMimeTypesManager *typeManager = new wxMimeTypesManager();
    wxFileType *htmlType = typeManager->GetFileTypeFromExtension(".html");
    wxString openCommand = htmlType->GetOpenCommand(file);
    if (openCommand.IsEmpty()) {
      wxMessageDialog cantBrowseMessage(this, "Couldn't open file",
          "Sorry, don't know how to open/browse files on your platform.", wxOK);
      cantBrowseMessage.ShowModal();
    } else {
      ::wxExecute(openCommand, wxEXEC_ASYNC, NULL);
    }
    delete htmlType;
    delete typeManager;
#endif
  }
}

void NewMatchDialog::onChangeBaseDir(wxCommandEvent &event) {
  changeBaseDir();
}

void NewMatchDialog::changeBaseDir() {
  chooseNewRootDir();
  updateBaseDirLabels();
  listener_->reloadBaseDirs();
}

void NewMatchDialog::updateBaseDirLabels() {
  stagesBaseDirValueLabel_->SetLabelText(condenseIfNecessary(getStagesDir()));
  shipsBaseDirValueLabel_->SetLabelText(condenseIfNecessary(getShipsDir()));
}

std::string NewMatchDialog::condenseIfNecessary(std::string s) {
  if (s.size() > 45) {
    std::string cs;
    cs.append(s.substr(0, 10));
    cs.append("...");
    cs.append(s.substr(s.size() - 32, 32));
    return cs;
  } else {
    return s;
  }
}

void NewMatchDialog::onEscape() {
  listener_->onEscape();
}

void NewMatchDialog::onSelectStage(wxUpdateUIEvent &event) {
  validateButtons();
}

void NewMatchDialog::onSelectShip(wxUpdateUIEvent &event) {
  validateButtons();
}

void NewMatchDialog::onSelectLoadedShip(wxUpdateUIEvent &event) {
  validateButtons();
}

void NewMatchDialog::previewSelectedStage() {
  wxArrayInt selectedStageIndex;
  stageSelect_->GetSelections(selectedStageIndex);
  if (selectedStageIndex.Count() > 0) {
    wxString selectedStage =
    stageSelect_->GetString(*(selectedStageIndex.begin()));
    char *stage = new char[selectedStage.length() + 1];
#ifdef __WINDOWS__
    strcpy(stage, selectedStage.c_str());
#else
    strcpy(stage, selectedStage.fn_str());
#endif
    listener_->previewStage(stage);
    delete stage;
  }
}

bool NewMatchDialog::stageSelectHasFocus() {
  return stageSelect_->HasFocus();
}

bool NewMatchDialog::shipsSelectHasFocus() {
  return shipsSelect_->HasFocus();
}

bool NewMatchDialog::loadedShipsSelectHasFocus() {
  return loadedShipsSelect_->HasFocus();
}

void NewMatchDialog::validateButtons() {
  if (numLoadedShips_ > 0) {
    validateButtonSelectedListBox(startButton_, stageSelect_);
  } else {
    startButton_->Disable();
  }

  validateButtonSelectedListBox(addArrow_, shipsSelect_);
  validateButtonSelectedListBox(removeArrow_, loadedShipsSelect_);
  validateButtonNonEmptyListBox(clearButton_, loadedShipsSelect_);
}

void NewMatchDialog::validateButtonNonEmptyListBox(wxButton *button,
                                                   wxListBox *listBox) {
  if (listBox->GetCount() > 0) {
    button->Enable();
  } else {
    button->Disable();
  }
}

void NewMatchDialog::validateButtonSelectedListBox(wxButton *button,
                                                   wxListBox *listBox) {
  wxArrayInt selectedIndex;
  listBox->GetSelections(selectedIndex);
  if (selectedIndex.Count() > 0) {
    button->Enable();
  } else {
    button->Disable();
  }
}

void NewMatchDialog::setMnemonicLabels(bool modifierDown) {
  // TODO: I'd rather it look like the button was pressed when you hit the
  //       shortcut, if possible. For now having trouble figuring out the
  //       wxButton::Command() call.
  if (modifierDown) {
#ifdef __WXOSX__
    clearButton_->SetLabel("C&lear \u2318L");
    refreshButton_->SetLabel("&Refresh \u2318R");
    startButton_->SetLabel("Start &Match \u2318M");
#else
    clearButton_->SetLabel("C&lear  alt-L");
    refreshButton_->SetLabel("&Refresh  alt-R");
    startButton_->SetLabel("Start &Match!  alt-M");
#endif
  } else {
    clearButton_->SetLabel("C&lear");
    refreshButton_->SetLabel("    &Refresh    ");
    startButton_->SetLabel("    Start &Match!    ");
  }
}

void NewMatchDialog::focusStageSelect() {
  stageSelect_->SetFocus();
}

NewMatchEventFilter::NewMatchEventFilter(NewMatchDialog *newMatchDialog) {
  newMatchDialog_ = newMatchDialog;
}

NewMatchEventFilter::~NewMatchEventFilter() {

}

int NewMatchEventFilter::FilterEvent(wxEvent& event) {
  bool modifierDown = false;
  wxKeyEvent *keyEvent = ((wxKeyEvent*) &event);
#if defined(__WXOSX__)
  modifierDown = keyEvent->ControlDown();
#elif defined(__WINDOWS__)
  modifierDown = keyEvent->AltDown();
#endif

  const wxEventType type = event.GetEventType();
  if (type == wxEVT_KEY_DOWN && newMatchDialog_->IsActive()) {
    newMatchDialog_->setMnemonicLabels(modifierDown);
    int keyCode = keyEvent->GetKeyCode();
    if (keyCode == WXK_ESCAPE
        || (keyEvent->GetUnicodeKey() == 'W' && keyEvent->ControlDown())) {
      newMatchDialog_->onEscape();
      return Event_Processed;
    } else if ((keyCode == WXK_SPACE || keyCode == WXK_RETURN)
               && newMatchDialog_->shipsSelectHasFocus()) {
      newMatchDialog_->addSelectedShips();
      return Event_Processed;
    } else if ((keyCode == WXK_SPACE || keyCode == WXK_BACK)
               && (newMatchDialog_->loadedShipsSelectHasFocus())) {
      newMatchDialog_->removeSelectedLoadedShips();
    } else if (keyCode == WXK_SPACE && newMatchDialog_->stageSelectHasFocus()) {
      newMatchDialog_->previewSelectedStage();
      return Event_Processed;
#ifdef __WXOSX__
    // Mac OS X doesn't handle mnemonics, so add some manual keyboard shortcuts.
    } else if (keyEvent->GetUnicodeKey() == 'M' && modifierDown) {
      newMatchDialog_->startMatch();
      return Event_Processed;
    } else if (keyEvent->GetUnicodeKey() == 'R' && modifierDown) {
      newMatchDialog_->refreshFiles();
      return Event_Processed;
    } else if (keyEvent->GetUnicodeKey() == 'L' && modifierDown) {
      newMatchDialog_->clearLoadedShips();
      return Event_Processed;
#endif
    }
  }

  if (type == wxEVT_KEY_UP) {
    newMatchDialog_->setMnemonicLabels(modifierDown);
  } else if (type == wxEVT_KILL_FOCUS) {
    newMatchDialog_->setMnemonicLabels(false);
  }

  return Event_Skip;
}
