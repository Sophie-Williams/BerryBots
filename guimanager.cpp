/*
  Copyright (C) 2012-2015 - Voidious

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

#include <algorithm>
#include <exception>
#include <sstream>
#include <SFML/Graphics.hpp>
#include <SFML/System/Vector2.hpp>
#include <wx/wx.h>
#include <wx/dataview.h>
#include <platformstl/filesystem/readdir_sequence.hpp>
#include "ResourcePath.hpp"
#include "bbutil.h"
#include "bblua.h"
#include "stage.h"
#include "bbengine.h"
#include "gfxeventhandler.h"
#include "gfxmanager.h"
#include "filemanager.h"
#include "gamerunner.h"
#include "guigamerunner.h"
#include "newmatch.h"
#include "stagepreview.h"
#include "packagedialog.h"
#include "packageship.h"
#include "packagestage.h"
#include "runnerdialog.h"
#include "outputconsole.h"
#include "resultsdialog.h"
#include "printhandler.h"
#include "guiprinthandler.h"
#include "basedir.h"
#include "bbwx.h"
#include "guizipper.h"
#include "replaybuilder.h"
#include "guimanager.h"

GuiManager::GuiManager(GuiListener *listener) {
  listener_ = listener;
  newMatchDialog_ = 0;
  srand((unsigned int) time(NULL));
  reloadBaseDirs();

  window_ = 0;
  previewWindow_ = 0;
  zipper_ = new GuiZipper();
  fileManager_ = new FileManager(zipper_);
  gameRunner_ = 0;
  menuBarMaker_ = new MenuBarMaker();
  packagingConsole_ = new OutputConsole("Packaging Details", CONSOLE_PLAIN,
                                        menuBarMaker_);
  errorConsole_ = new OutputConsole("Error Console", CONSOLE_PLAIN,
                                    menuBarMaker_);
  runnerConsole_ = new OutputConsole("Game Runner Console", CONSOLE_RUNNER,
                                     menuBarMaker_);
  runnerConsoleListener_ = new RunnerConsoleListener(this);
  runnerConsole_->setListener(runnerConsoleListener_);
  packagingConsole_->SetPosition(wxPoint(150, 100));
  newMatchListener_ = new MatchStarter(this);
  newMatchDialog_ = new NewMatchDialog(newMatchListener_, menuBarMaker_);
  resultsDialog_ = 0;
  shipPackager_ = new ShipPackager(this, fileManager_, packagingConsole_);
  packageShipDialog_ = new PackageShipDialog(shipPackager_, menuBarMaker_);
  stagePackager_ = new StagePackager(this, fileManager_, packagingConsole_);
  packageStageDialog_ = new PackageStageDialog(stagePackager_, menuBarMaker_);
  runnerLauncher_ = new RunnerLauncher(this, fileManager_, runnerConsole_);
  runnerDialog_ = new RunnerDialog(runnerLauncher_, menuBarMaker_);
  loadStages();
  loadShips();
  loadRunners();

  guiPrintHandler_ = 0;
  stageConsole_ = 0;
  teamConsoles_ = 0;
  stagePreview_ = new StagePreview(menuBarMaker_);
  stagePreview_->setListener(new PreviewInputListener(this));
  gfxManager_ = new GfxManager(resourcePath(), true);
  viewListener_ = new ViewListener(this);
  gfxManager_->setListener(viewListener_);
  packageReporter_ = new PackageReporter(packagingConsole_);
  fileManager_->setListener(packageReporter_);
  newMatchDialog_->Show();
  newMatchDialog_->SetFocus();
  engine_ = 0;
  gfxHandler_ = 0;
  currentStagePath_ = 0;
  currentTeamPaths_ = 0;
  currentNumTeams_ = 0;
  interrupted_ = false;
  paused_ = false;
  restarting_ = false;
  quitting_ = false;
  showedResults_ = false;
  runnerRunning_ = false;
  nextWindow_ = 0;
  tpsFactor_ = 0.5;
  nextDrawTime_ = 1;
  numStages_ = numShips_ = numRunners_ = 0;

#ifdef __WINDOWS__
  windowIcon_.loadFromFile(resourcePath() + BBICON_32);
#elif defined(__WXGTK__)
  windowIcon_.loadFromFile(resourcePath() + BBICON_128);
#endif
}

GuiManager::~GuiManager() {
  deleteCurrentMatchSettings();
  destroyStageConsole();
  destroyResultsDialog();
  newMatchDialog_->Destroy();
  packageShipDialog_->Destroy();
  packageStageDialog_->Destroy();
  runnerDialog_->Destroy();
  packagingConsole_->Destroy();
  errorConsole_->Destroy();
  runnerConsole_->Destroy();
  stagePreview_->Destroy();
  delete runnerConsoleListener_;
  delete menuBarMaker_;
  if (engine_ != 0) {
    delete engine_;
  }
  if (window_ != 0) {
    delete window_;
  }
  if (previewWindow_ != 0) {
    delete previewWindow_;
  }
  if (gfxHandler_ != 0) {
    delete gfxHandler_;
  }
  delete gfxManager_;
  delete viewListener_;
  delete zipper_;
  delete fileManager_;
  delete newMatchListener_;
  delete shipPackager_;
  delete stagePackager_;
  delete runnerLauncher_;
  delete packageReporter_;
  if (guiPrintHandler_ != 0) {
    delete guiPrintHandler_;
  }
}

void GuiManager::reloadBaseDirs() {
  if (newMatchDialog_ != 0) {
    newMatchDialog_->onSetBaseDirs();
  }
}

void GuiManager::loadStages() {
  newMatchDialog_->clearStages();
  packageStageDialog_->clearItems();
  BerryBotsEngine engine(0, fileManager_, 0);
  numStages_ = loadItemsFromDir(getStagesDir().c_str(), getStagesDir().c_str(),
                                ITEM_STAGE, packageStageDialog_, &engine);
}

bool GuiManager::isValidStageFile(const char *srcFilename,
                                  BerryBotsEngine *engine) {
  // TODO: Is this too slow? Should we keep this list in the cache so we don't
  //       have to do this on every startup / refresh - at least for packaged
  //       stages? In fact, just the presence in the cache could be considered
  //       a sign of validity.
  // TODO: Move this out of the GUI code.
  if (fileManager_->isLuaFilename(srcFilename)
      || fileManager_->isZipFilename(srcFilename)) {
    char *stagesDir = 0;
    char *stageFilename = 0;
    try {
      fileManager_->loadStageFileData(getStagesDir().c_str(), srcFilename,
                                      &stagesDir, &stageFilename,
                                      getCacheDir().c_str());
    } catch (FileNotFoundException *fnfe) {
      // Only possible if user deletes file from disk after we find it on disk
      // but before we validate it. Seems safe to fail silently.
      if (stagesDir != 0) {
        delete stagesDir;
      }
      if (stageFilename != 0) {
        delete stageFilename;
      }
      delete fnfe;
      return false;
    } catch (ZipperException *ze) {
      if (stagesDir != 0) {
        delete stagesDir;
      }
      if (stageFilename != 0) {
        delete stageFilename;
      }
      errorConsole_->print(srcFilename);
      errorConsole_->print(": ");
      errorConsole_->println(ze->what());
      wxMessageDialog errorMessage(NULL, ze->what(), "Unzip failure",
                                   wxOK | wxICON_EXCLAMATION);
      errorMessage.ShowModal();
      delete ze;
      return false;
    } catch (PackagedSymlinkException *pse) {
      if (stagesDir != 0) {
        delete stagesDir;
      }
      if (stageFilename != 0) {
        delete stageFilename;
      }
      errorConsole_->print(srcFilename);
      errorConsole_->print(": ");
      errorConsole_->println(pse->what());
      delete pse;
      return false;
    }
    lua_State *stageState;
    initStageState(&stageState, stagesDir);

    if (luaL_loadfile(stageState, stageFilename)
        || engine->callUserLuaCode(stageState, 0, "", PCALL_VALIDATE)) {
      logErrorMessage(stageState, "Problem loading stage: %s");
      lua_close(stageState);
      delete stagesDir;
      delete stageFilename;
      return false;
    }

    lua_getglobal(stageState, "configure");
    if (lua_isnil(stageState, -1)) {
      lua_close(stageState);
      delete stagesDir;
      delete stageFilename;
      return false;
    }

    lua_close(stageState);
    delete stagesDir;
    delete stageFilename;
    return true;
  }
  return false;
}

void GuiManager::loadShips() {
  newMatchDialog_->clearShips();
  packageShipDialog_->clearItems();
  BerryBotsEngine engine(0, fileManager_, 0);
  numShips_ = loadItemsFromDir(getShipsDir().c_str(), getShipsDir().c_str(),
                               ITEM_SHIP, packageShipDialog_, &engine);
  newMatchDialog_->removeStaleLoadedShips();
}

bool GuiManager::isValidShipFile(const char *srcFilename,
                                 BerryBotsEngine *engine) {
  // TODO: Is this too slow? Should we keep this list in the cache so we don't
  //       have to do this on every startup / refresh - at least for packaged
  //       ships? In fact, just the presence in the cache could be considered
  //       a sign of validity.
  // TODO: Move this out of the GUI code.
  if (fileManager_->isLuaFilename(srcFilename)
      || fileManager_->isZipFilename(srcFilename)) {
    char *shipDir = 0;
    char *shipFilename = 0;
    try {
      fileManager_->loadShipFileData(getShipsDir().c_str(), srcFilename,
                                     &shipDir, &shipFilename,
                                     getCacheDir().c_str());
    } catch (FileNotFoundException *fnfe) {
      // Only possible if user deletes file from disk after we find it on disk
      // but before we validate it. Seems safe to fail silently.
      if (shipDir != 0) {
        delete shipDir;
      }
      if (shipFilename != 0) {
        delete shipFilename;
      }
      delete fnfe;
      return false;
    } catch (ZipperException *ze) {
      if (shipDir != 0) {
        delete shipDir;
      }
      if (shipFilename != 0) {
        delete shipFilename;
      }
      errorConsole_->print(srcFilename);
      errorConsole_->print(": ");
      errorConsole_->println(ze->what());
      wxMessageDialog errorMessage(NULL, ze->what(), "Unzip failure",
                                   wxOK | wxICON_EXCLAMATION);
      errorMessage.ShowModal();
      delete ze;
      return false;
    } catch (PackagedSymlinkException *pse) {
      if (shipDir != 0) {
        delete shipDir;
      }
      if (shipFilename != 0) {
        delete shipFilename;
      }
      errorConsole_->print(srcFilename);
      errorConsole_->print(": ");
      errorConsole_->println(pse->what());
      delete pse;
      return false;
    }
    lua_State *shipState;
    initShipState(&shipState, shipDir);

    if (luaL_loadfile(shipState, shipFilename)
        || engine->callUserLuaCode(shipState, 0, "", PCALL_VALIDATE)) {
      logErrorMessage(shipState, "Problem loading ship: %s");
      lua_close(shipState);
      delete shipDir;
      delete shipFilename;
      return false;
    }

    lua_getglobal(shipState, "configure");
    lua_getglobal(shipState, "init");
    if (lua_isnil(shipState, -1) || !lua_isnil(shipState, -2)) {
      lua_close(shipState);
      delete shipDir;
      delete shipFilename;
      return false;
    }
    
    lua_close(shipState);
    delete shipDir;
    delete shipFilename;
    return true;
  }
  return false;
}

void GuiManager::loadRunners() {
  runnerDialog_->clearItems();
  BerryBotsEngine engine(0, fileManager_, 0);

  numRunners_ = loadItemsFromDir(getRunnersDir().c_str(),
                                 getRunnersDir().c_str(), ITEM_RUNNER,
                                 runnerDialog_, &engine);
}

bool GuiManager::isValidRunnerFile(const char *srcFilename,
                                   BerryBotsEngine *engine) {
  // TODO: Move this out of the GUI code.
  if (fileManager_->isLuaFilename(srcFilename)) {
    char *runnersDir = new char[strlen(getRunnersDir().c_str()) + 1];
    strcpy(runnersDir, getRunnersDir().c_str());
    lua_State *runnerState;
    initRunnerState(&runnerState, runnersDir);

    if (luaL_loadfile(runnerState, srcFilename)
        || engine->callUserLuaCode(runnerState, 0, "", PCALL_VALIDATE)) {
      logErrorMessage(runnerState, "Problem loading runner: %s");
      lua_close(runnerState);
      delete runnersDir;
      return false;
    }

    lua_getglobal(runnerState, "run");
    if (lua_isnil(runnerState, -1)) {
      lua_close(runnerState);
      delete runnersDir;
      return false;
    }
    
    lua_close(runnerState);
    delete runnersDir;
    return true;
  }
  return false;
}

// TODO: Factor out a common interface or base class instead of passing (void*).
int GuiManager::loadItemsFromDir(const char *baseDir, const char *loadDir,
    int itemType, void *dialog, BerryBotsEngine *engine) {
  int numItems = 0;
  platformstl::readdir_sequence dir(loadDir,
      platformstl::readdir_sequence::files
          | platformstl::readdir_sequence::directories);
  platformstl::readdir_sequence::const_iterator first = dir.begin();
  platformstl::readdir_sequence::const_iterator last = dir.end();
  while (first != last) {
    platformstl::readdir_sequence::const_iterator file = first++;
    char *filename = (char *) *file;
    char *filePath = fileManager_->getFilePath(loadDir, filename);
    if (fileManager_->isDirectory(filePath)) {
      numItems += loadItemsFromDir(baseDir, filePath, itemType, dialog, engine);
    } else {
      char *relativeFilename = &(filePath[strlen(baseDir) + 1]);
      bool valid = false;
      if (itemType == ITEM_SHIP && isValidShipFile(relativeFilename, engine)) {
        newMatchDialog_->addShip(relativeFilename);
        valid = true;
      } else if (itemType == ITEM_STAGE
                 && isValidStageFile(relativeFilename, engine)) {
        newMatchDialog_->addStage(relativeFilename);
        valid = true;
      } else if (itemType == ITEM_RUNNER
                 && isValidRunnerFile(relativeFilename, engine)) {
        valid = true;
      }
      if (valid) {
        numItems++;
        if (fileManager_->isLuaFilename(filename)) {
          if (itemType == ITEM_RUNNER) {
            ((RunnerDialog *) dialog)->addItem(relativeFilename);
          } else {
            ((PackageDialog *) dialog)->addItem(relativeFilename);
          }
        }
      }
    }
    delete filePath;
  }
  return numItems;
}

sf::RenderWindow* GuiManager::initMainWindow(unsigned int width,
                                             unsigned int height) {
  if (window_ != 0) {
    sf::RenderWindow *oldWindow = window_;
    window_ = 0;
    delete oldWindow;
  }

  window_ = new sf::RenderWindow(sf::VideoMode(width, height), "BerryBots",
      sf::Style::Default,
      sf::ContextSettings(0, 0, (isAaDisabled() ? 0 : 16), 2, 0));

#ifdef __WINDOWS__
  window_->setIcon(32, 32, windowIcon_.getPixelsPtr());
#elif defined(__WXGTK__)
  window_->setIcon(128, 128, windowIcon_.getPixelsPtr());
#endif

  return window_;
}

sf::RenderWindow* GuiManager::initPreviewWindow(unsigned int width,
                                                unsigned int height) {
  if (previewWindow_ != 0) {
    sf::RenderWindow *oldWindow = previewWindow_;
    previewWindow_ = 0;
    delete oldWindow;
  }

  previewWindow_ = new sf::RenderWindow(sf::VideoMode(width, height), "Preview",
      sf::Style::None,
      sf::ContextSettings(0, 0, (isAaDisabled() ? 0 : 4), 2, 0));
  previewWindow_->setVisible(false);

#ifdef __WINDOWS__
  previewWindow_->setIcon(32, 32, windowIcon_.getPixelsPtr());
#elif defined(__WXGTK__)
  previewWindow_->setIcon(128, 128, windowIcon_.getPixelsPtr());
#endif

  return previewWindow_;
}

void GuiManager::startMatch(const char *stageName, char **teamNames,
                            int numUserTeams) {
  stagePreview_->Hide();
  do {
    runNewMatch(stageName, teamNames, numUserTeams);
  } while (restarting_);
}

void GuiManager::runNewMatch(const char *stageName, char **teamNames,
                             int numUserTeams) {
  tpsFactor_ = 0.5;
  nextDrawTime_ = 1;
  showedResults_ = false;

  sf::RenderWindow *window;
  bool maintainWindowScale = false;
  double prevScale = 1.0;
  double backingScale = getBackingScaleFactor();
  int screenWidth =
      backingScale * sf::VideoMode::getDesktopMode().width;
  int screenHeight =
      backingScale * sf::VideoMode::getDesktopMode().height;
  int dockSize = backingScale * DOCK_SIZE;

  if (window_ != 0) {
    if (restarting_) {
      prevScale = ((double) (window_->getSize().x - dockSize)) / viewWidth_;
      maintainWindowScale = true;
    }
  }
  
  // On Mac OS X, we need to init SFML before the wxWidgets stuff below or we
  // hit some unexplainable crashes when we delete an SFML window. I don't know
  // why, I've merely devised a work-around. Judging from some SFML forum
  // threads, it sounds likely to be an issue with nightmare-ish video drivers.
  if (window_ == 0) {
    window = initMainWindow(backingScale * screenWidth,
                            backingScale * (screenHeight - 75));
  } else {
    window = window_;
  }

  if (!restarting_) {
    saveCurrentMatchSettings(stageName, teamNames, numUserTeams);
  }
  if (engine_ != 0) {
    delete engine_;
    engine_ = 0;
  }

  if (restarting_) {
    stageConsole_->clear();
    guiPrintHandler_->restartMode();
  } else {
    destroyStageConsole();
    stageConsole_ = new OutputConsole(stageName, CONSOLE_SHIP_STAGE,
                                      menuBarMaker_);
    stageConsole_->Hide();

    if (guiPrintHandler_ != 0) {
      delete guiPrintHandler_;
      guiPrintHandler_ = 0;
    }
    guiPrintHandler_ = new GuiPrintHandler(stageConsole_, 0, menuBarMaker_);
  }

  engine_ = new BerryBotsEngine(guiPrintHandler_, fileManager_,
                                resourcePath().c_str());

  Stage *stage = engine_->getStage();
  if (restarting_) {
    stage->setGfxEnabled(stageConsole_->isChecked());
  }
  stageConsole_->setListener(new StageConsoleListener(stage));
  try {
    engine_->initStage(getStagesDir().c_str(), stageName,
                       getCacheDir().c_str());
    engine_->initShips(getShipsDir().c_str(), teamNames, numUserTeams,
                       getCacheDir().c_str());
    teamConsoles_ = guiPrintHandler_->getTeamConsoles();
  } catch (EngineException *e) {
    errorConsole_->print(stageName);
    errorConsole_->print(": ");
    errorConsole_->println(e->what());
    wxMessageDialog errorMessage(NULL, e->what(),
        "BerryBots engine init failed", wxOK | wxICON_EXCLAMATION);
    errorMessage.ShowModal();
    delete engine_;
    engine_ = 0;
    restarting_ = false;
    newMatchDialog_->Show();
    delete e;
    return;
  }

  viewWidth_ = stage->getWidth() + (STAGE_MARGIN * 2);
  viewHeight_ = stage->getHeight() + (STAGE_MARGIN * 2);
  double windowScale;
  if (restarting_ && maintainWindowScale) {
    windowScale = prevScale;
  } else {
    windowScale =
        std::min(backingScale, std::min(
            ((double) screenWidth - dockSize) / viewWidth_,
            ((double) screenHeight) / viewHeight_));
  }
  unsigned int targetWidth = round(windowScale * viewWidth_) + dockSize;
  unsigned int targetHeight = round(windowScale * viewHeight_);

  window->setSize(sf::Vector2u(targetWidth, targetHeight));

  interrupted_ = false;
  paused_ = false;
  newMatchDialog_->Hide();
  packageStageDialog_->Hide();
  packageShipDialog_->Hide();
  gfxHandler_ = new GfxEventHandler();
  stage->addEventHandler((EventHandler*) gfxHandler_);

  // TODO: If/when SFML getPosition() works, adjust the window position to
  //       keep the whole window on the screen (if necessary). Might be worth
  //       platform-specific implementations using getSystemHandle() if that
  //       doesn't happen in a reasonable timeframe.
  //       We could just set it to (0, 0) or centered on screen every time, but
  //       that seems potentially super annoying to a user - less annoying than
  //       having to move the window occasionally if you switch to a bigger
  //       stage that goes off-screen.

  gfxManager_->initBbGfx(window, backingScale, viewHeight_, stage,
                         engine_->getTeams(), engine_->getNumTeams(),
                         engine_->getShips(), engine_->getNumShips());
  gfxManager_->initViews(window, viewWidth_, viewHeight_);
  window->setVisible(true);
  drawFrame(window);

#ifdef __WXOSX__
  // SFML 2.1+ has a weird scaling issue. Some of it is documented here:
  // https://github.com/SFML/SFML/issues/474
  // Basically, OS X caps the window height, but SFML reports the requested
  // height instead of the actual height. My work-around is to manually trigger
  // a resize after drawing one frame.
  processMainWindowEvents(window, gfxManager_, viewWidth_, viewHeight_);
  gfxManager_->onResize(window, viewWidth_, viewHeight_);
#endif

  runCurrentMatch();
}

// TODO: Track and display TPS in GUI.
void GuiManager::runCurrentMatch() {
  interrupted_ = false;
  restarting_ = false;
  runnerConsole_->Hide();
  destroyResultsDialog();
  sf::RenderWindow *window = window_;
  try {
    while (window->isOpen() && !interrupted_ && !restarting_ && !quitting_) {
      while (!paused_ && !restarting_ && !engine_->isGameOver()
          && engine_->getGameTime() < nextDrawTime_) {
        engine_->processTick();
      }
      
      while (!interrupted_ && !restarting_ && !quitting_
             && (nextDrawTime_ <= engine_->getGameTime()
                 || engine_->isGameOver())) {
        processMainWindowEvents(window, gfxManager_, viewWidth_, viewHeight_);
        clearTeamErroredForActiveConsoles(engine_);
        drawFrame(window);
        if (!paused_ && !engine_->isGameOver()) {
          nextDrawTime_ += tpsFactor_;
        }
        if (engine_->isGameOver() && !showedResults_) {
          ReplayBuilder *replayBuilder = engine_->getReplayBuilder();
          Team **teams = engine_->getRankedTeams();
          replayBuilder->setResults(teams, engine_->getNumTeams());
          delete teams;
          showResults(replayBuilder);
          showedResults_ = true;
        }
      }
    }
  } catch (EngineException *e) {
    errorConsole_->println(e->what());
    wxMessageDialog errorMessage(NULL, e->what(),
        "BerryBots encountered an error", wxOK | wxICON_EXCLAMATION);
    errorMessage.ShowModal();
    newMatchDialog_->Show();
    delete e;
    return;
  }

  if (!window->isOpen()) {
    listener_->onAllWindowsClosed();
  }

  // TODO: Display CPU usage in GUI

  if (!interrupted_) {
    gfxManager_->destroyBbGfx();
    delete engine_;
    engine_ = 0;
    delete gfxHandler_;
    gfxHandler_ = 0;
  }
}

void GuiManager::drawFrame(sf::RenderWindow *window) {
  window->clear();
  gfxManager_->drawGame(window, engine_->getStage(), engine_->getShips(),
                        engine_->getNumShips(), engine_->getGameTime(),
                        gfxHandler_, paused_, engine_->isGameOver(),
                        engine_->getWinnerName());
  window->display();
}

void GuiManager::showResults(ReplayBuilder *replayBuilder) {
  Team **rankedTeams = engine_->getRankedTeams();
  sf::Vector2u windowSize = window_->getSize();
  sf::Vector2i windowPosition = window_->getPosition();
  int xCenter = windowPosition.x + (windowSize.x / 2);
  int yCenter = windowPosition.y + (windowSize.y / 2);

  // SFML 2.3 is returning wrong/negative values for y coordinate on Mac OS X.
  // If results dialog will end up off-screen, use center of screen instead.
  unsigned int screenWidth = sf::VideoMode::getDesktopMode().width;
  unsigned int screenHeight = sf::VideoMode::getDesktopMode().height;
  if (xCenter < 0 || xCenter > screenWidth || yCenter < 0
      || yCenter > screenHeight) {
    xCenter = screenWidth / 2;
    yCenter = screenHeight / 2;
  }

  resultsDialog_ = new ResultsDialog(engine_->getStage()->getName(),
      rankedTeams, engine_->getNumTeams(), engine_->hasScores(),
      wxPoint(xCenter, yCenter), replayBuilder, new ResultsRestarter(this));
  delete rankedTeams;
  resultsDialog_->Show();
  resultsDialog_->Raise();
}

void GuiManager::clearTeamErroredForActiveConsoles(BerryBotsEngine *engine) {
  for (int x = 0; x < engine_->getNumTeams(); x++) {
    Team *team = engine_->getTeam(x);
    if (team->errored && teamConsoles_[x]->IsActive()) {
      team->errored = false;
    }
  }
}

void GuiManager::resumeMatch() {
  if (interrupted_) {
    gfxManager_->hideKeyboardShortcuts();
    hideNewMatchDialog();
    hidePackageShipDialog();
    hidePackageStageDialog();
    hideGameRunnerDialog();
    hidePackagingConsole();
    hideErrorConsole();
    stagePreview_->Hide();
    runCurrentMatch();
  }
  while (restarting_) {
    runNewMatch(currentStagePath_, currentTeamPaths_, currentNumTeams_);
  }
}

void GuiManager::processMainWindowEvents(sf::RenderWindow *window,
    GfxManager *gfxManager, int viewWidth, int viewHeight) {
  sf::Event event;
  bool resized = false;
  while (window->pollEvent(event)) {
    bool modifierDown = (event.key.system || event.key.control);

#ifdef __WXOSX__
    // For some reason, the dash key (between 0 and =) generates no KeyPressed
    // event on Mac OS X, but it does generate a TextEntered event. Also, as of
    // Yosemite + SFML 2.3, we no longer receive the dash key at all when cmd or
    // ctrl is pressed. So on OS X, just look for +, -, and 0 keys with no
    // modifier, to be consistent.
    if (event.type == sf::Event::TextEntered && event.text.unicode == 45) {
      gfxManager->decreaseWindowSize(window, viewWidth, viewHeight);
    }
    modifierDown = true;
#endif

    if (event.type == sf::Event::Closed) {
      window->close();
      quit();
    }
    if (event.type == sf::Event::MouseWheelMoved) {
      sf::Event::MouseWheelEvent wheelEvent = event.mouseWheel;
      gfxManager->processMouseWheel(wheelEvent.x, wheelEvent.y,
                                    wheelEvent.delta);
    }
    if (event.type == sf::Event::Resized && !resized) {
      resized = true;
      gfxManager->onResize(window, viewWidth, viewHeight);
    }
    if (event.type == sf::Event::MouseButtonPressed) {
      gfxManager->processMouseDown(event.mouseButton.x, event.mouseButton.y);
    }
    if (event.type == sf::Event::MouseButtonReleased) {
      gfxManager->processMouseUp(event.mouseButton.x, event.mouseButton.y);
    }
    if (event.type == sf::Event::MouseMoved
        || event.type == sf::Event::MouseEntered) {
      gfxManager->processMouseMoved(event.mouseMove.x, event.mouseMove.y);
    }
    if (event.type == sf::Event::MouseLeft) {
      gfxManager->processMouseMoved(-1, -1);
    }
    if (event.type == sf::Event::KeyPressed) {
      switch (event.key.code) {
        case sf::Keyboard::Space:
          togglePause();
          break;
        case sf::Keyboard::BackSpace:
          restartMatch();
          break;
        case sf::Keyboard::Escape:
          showNewMatchDialog();
          break;
#ifdef __WXOSX__
        case sf::Keyboard::LSystem:
        case sf::Keyboard::RSystem:
          gfxManager->showKeyboardShortcuts();
          break;
#else
        case sf::Keyboard::LAlt:
        case sf::Keyboard::RAlt:
          gfxManager->showKeyboardShortcuts();
          break;
#endif
        case sf::Keyboard::N:
          showNewMatchDialog();
          break;
        case sf::Keyboard::P:
          showPackageShipDialog();
          break;
        case sf::Keyboard::T:
          showPackageStageDialog();
          break;
        case sf::Keyboard::G:
          showGameRunnerDialog();
          break;
        case sf::Keyboard::Equal:
        case sf::Keyboard::Add:
          if (modifierDown) {
            gfxManager->increaseWindowSize(window, viewWidth, viewHeight);
          }
          break;
        case sf::Keyboard::Dash:
        case sf::Keyboard::Subtract:
          if (modifierDown) {
            gfxManager->decreaseWindowSize(window, viewWidth, viewHeight);
          }
          break;
        case sf::Keyboard::Num0:
          if (modifierDown) {
            gfxManager->defaultWindowSize(window, viewWidth, viewHeight);
          }
          break;
        case sf::Keyboard::LBracket:
          gfxManager->decreaseGameSpeed();
          break;
        case sf::Keyboard::RBracket:
          gfxManager->increaseGameSpeed();
          break;
        default:
          break;
      }
    }

    if (event.type == sf::Event::KeyReleased) {
      switch (event.key.code) {
#ifdef __WXOSX__
        case sf::Keyboard::LSystem:
        case sf::Keyboard::RSystem:
          gfxManager->hideKeyboardShortcuts();
          break;
#else
        case sf::Keyboard::LAlt:
        case sf::Keyboard::RAlt:
          gfxManager->hideKeyboardShortcuts();
          break;
#endif
        default:
          break;
      }
    }

    // On Mac/Cocoa, when using a different Space, the rest of the OS UI slows
    // to a crawl unless you have a frame rate limit set. But the frame rate is
    // smoother if we use vsync instead of a fixed frame rate, so do that when
    // we have focus.
    // TODO: Determine if this is necessary/preferable on Linux/Windows.
    // TODO: Might be better to restrict this to the Spaces case specifically,
    //       or when window isn't visible to user.
    if (event.type == sf::Event::LostFocus) {
      window->setVerticalSyncEnabled(false);
      window->setFramerateLimit(paused_ ? 5 : 60);
    } else if (event.type == sf::Event::GainedFocus) {
      updateFramerate();
    }
  }

  // On Linux/GTK and Windows, the wxWidgets windows don't get events while
  // this thread has control unless we wxYield each frame. Seems to be
  // unnecessary on Mac/Cocoa.
#ifndef __WXOSX__
  wxYield();
#endif
}

void GuiManager::launchGameRunner(const char *runnerName) {
  loadStages();
  loadShips();
  char **stageNames = newMatchDialog_->getStageNames();
  char **shipNames = newMatchDialog_->getShipNames();
  GuiPrintHandler *printHandler =
      new GuiPrintHandler(0, runnerConsole_, menuBarMaker_);
  gameRunner_ = new GuiGameRunner(printHandler, stageNames, numStages_,
      shipNames, numShips_, zipper_, resourcePath().c_str(),
      new RunnerFormRedrawer(this));
  runnerDialog_->Hide();
  nextWindow_ = 0;
  runnerRunning_ = true;

  runnerConsole_->clear();
  runnerConsole_->Show();
  runnerConsole_->Raise();

  gameRunner_->run(runnerName);
  runnerRunning_ = false;

  GuiGameRunner *oldRunner = gameRunner_;
  gameRunner_ = 0;
  delete oldRunner;
  delete printHandler;
  for (int x = 0; x < numStages_; x++) {
    delete stageNames[x];
  }
  delete stageNames;
  for (int x = 0; x < numShips_; x++) {
    delete shipNames[x];
  }
  delete shipNames;

  switch (nextWindow_) {
    case NEXT_NEW_MATCH:
      showNewMatchDialog();
      break;
    case NEXT_PACKAGE_SHIP:
      showPackageShipDialog();
      break;
    case NEXT_PACKAGE_STAGE:
      showPackageStageDialog();
      break;
    case NEXT_GAME_RUNNER:
      showGameRunnerDialog();
      break;
    case NEXT_RESUME_MATCH:
      resumeMatch();
      break;
    default:
      break;
  }
}

void GuiManager::abortGameRunner() {
  if (runnerRunning_) {
    nextWindow_ = NEXT_GAME_RUNNER;
  } else {
    showGameRunnerDialog();
  }
  // TODO: pretty sure we need a mutex here
  if (gameRunner_ != 0) {
    gameRunner_->quit();
  }
}

void GuiManager::showNewMatchDialog() {
  if (confirmDialogSwitch(NEXT_NEW_MATCH)) {
    showDialog(newMatchDialog_);
  }
}

void GuiManager::showPackageShipDialog() {
  if (confirmDialogSwitch(NEXT_PACKAGE_SHIP)) {
    showDialog(packageShipDialog_);
  }
}

void GuiManager::showPackageStageDialog() {
  if (confirmDialogSwitch(NEXT_PACKAGE_STAGE)) {
    showDialog(packageStageDialog_);
  }
}

void GuiManager::showGameRunnerDialog() {
  if (confirmDialogSwitch(NEXT_GAME_RUNNER)) {
    showDialog(runnerDialog_);
  }
}

void GuiManager::showDialog(wxFrame *dialog) {
  interrupted_ = true;
  if (newMatchDialog_ != dialog && newMatchDialog_->IsShown()) {
    newMatchDialog_->Hide();
  }
  if (packageShipDialog_ != dialog && packageShipDialog_->IsShown()) {
    packageShipDialog_->Hide();
  }
  if (packageStageDialog_ != dialog && packageStageDialog_->IsShown()) {
    packageStageDialog_->Hide();
  }
  if (runnerDialog_ != dialog && runnerDialog_->IsShown()) {
    runnerDialog_->Hide();
  }
  dialog->Show();
  dialog->Raise();
}

bool GuiManager::confirmDialogSwitch(int nextWindow) {
  if (runnerRunning_) {
    wxMessageDialog confirmMessage(NULL,
        "This will abort the currently running Game Runner. Are you sure?",
        "Abort Game Runner", wxOK | wxCANCEL | wxICON_EXCLAMATION);
    int r = confirmMessage.ShowModal();
    if (r == wxID_OK) {
      nextWindow_ = nextWindow;
      gameRunner_->quit();
    }
    return false;
  }
  return true;
}

void GuiManager::showStageConsole() {
  stageConsole_->Show();
  stageConsole_->Raise();
}

void GuiManager::showTeamConsole(int teamIndex) {
  teamConsoles_[teamIndex]->Show();
  teamConsoles_[teamIndex]->Raise();
}

void GuiManager::showErrorConsole() {
  errorConsole_->Show();
  errorConsole_->Raise();
}

void GuiManager::showStagePreview(const char *stageName) {
#ifndef __WXGTK__
  // On Mac OS X - or my MacBook Pros, anyway - we absolutely have to new up the
  // SFML window before doing any wxWidgets stuff when we're triggered from an
  // event like this. If we selectStage() first, for instance, the app will
  // crash when we delete that window.
  //
  // On Mac and Windows it's a better experience to use a persistent window. On
  // Linux, setSize() doesn't work reliably on the SFML window after it's
  // created, so we create a new one each time (in stagepreview.cpp).
  if (previewWindow_ == 0) {
    previewWindow_ = initPreviewWindow(50, 50);
  }
#endif

  wxPoint newMatchPosition = newMatchDialog_->GetPosition();
  selectStage(stageName);
  stagePreview_->showPreview(previewWindow_, stageName,
                             newMatchPosition.x + 50, newMatchPosition.y + 50);
  stagePreview_->Show();
  stagePreview_->Raise();
}

void GuiManager::closeStagePreview() {
  stagePreview_->Hide();
  newMatchDialog_->Show();
  newMatchDialog_->Raise();
  newMatchDialog_->focusStageSelect();
}

void GuiManager::previewNextStage() {
  newMatchDialog_->previewNextStage();
}

void GuiManager::previewPreviousStage() {
  newMatchDialog_->previewPreviousStage();
}

void GuiManager::selectStage(const char *stageName) {
  newMatchDialog_->selectStage(stageName);
}

void GuiManager::destroyStageConsole() {
  if (stageConsole_ != 0) {
    stageConsole_->Hide();
    stageConsole_->Destroy();
    stageConsole_ = 0;
  }
}

void GuiManager::destroyResultsDialog() {
  if (resultsDialog_ != 0) {
    resultsDialog_->Hide();
    resultsDialog_->Destroy();
    resultsDialog_ = 0;
  }
}

void GuiManager::hideNewMatchDialog() {
  newMatchDialog_->Hide();
}

void GuiManager::hidePackageShipDialog() {
  packageShipDialog_->Hide();
}

void GuiManager::hidePackageStageDialog() {
  packageStageDialog_->Hide();
}

void GuiManager::hideGameRunnerDialog() {
  runnerDialog_->Hide();
}

void GuiManager::hidePackagingConsole() {
  packagingConsole_->Hide();
}

void GuiManager::hideErrorConsole() {
  errorConsole_->Hide();
}

void GuiManager::dialogClosed() {
  if (window_ == 0) {
    listener_->onAllWindowsClosed();
  } else if (runnerRunning_) {
    nextWindow_ = NEXT_RESUME_MATCH;
  } else {
    resumeMatch();
  }
}

void GuiManager::dialogEscaped() {
  if (window_ != 0) {
    resumeMatch();
  }
}

void GuiManager::newMatchInitialFocus() {
  newMatchDialog_->focusStageSelect();
}

void GuiManager::packageShipInitialFocus() {
  packageShipDialog_->focusItemSelect();
}

void GuiManager::packageStageInitialFocus() {
  packageStageDialog_->focusItemSelect();
}

void GuiManager::gameRunnerInitialFocus() {
  runnerDialog_->focusItemSelect();
}

void GuiManager::saveCurrentMatchSettings(
    const char *stagePath, char **teamPaths, int numTeams) {
  deleteCurrentMatchSettings();
  currentStagePath_ = new char[strlen(stagePath) + 1];
  strcpy(currentStagePath_, stagePath);
  currentTeamPaths_ = new char*[numTeams];
  for (int x = 0; x < numTeams; x++) {
    currentTeamPaths_[x] = new char[strlen(teamPaths[x]) + 1];
    strcpy(currentTeamPaths_[x], teamPaths[x]);
  }
  currentNumTeams_ = numTeams;
}

void GuiManager::deleteCurrentMatchSettings() {
  if (currentStagePath_ != 0) {
    delete currentStagePath_;
    currentStagePath_ = 0;
  }
  if (currentTeamPaths_ != 0) {
    for (int x = 0; x < currentNumTeams_; x++) {
      delete currentTeamPaths_[x];
    }
    delete currentTeamPaths_;
    currentTeamPaths_ = 0;
  }
}

void GuiManager::togglePause() {
  paused_ = !paused_;
}

void GuiManager::restartMatch() {
  restarting_ = true;
}

void GuiManager::setTpsFactor(double tpsFactor) {
  tpsFactor_ = round(tpsFactor, 3);
  paused_ = (tpsFactor_ < 0.005);
  updateFramerate();
}

void GuiManager::updateFramerate() {
  sf::RenderWindow *window = window_;
  bool defaultTps = (abs(tpsFactor_ - 1) < 0.01);
  if (defaultTps) {
    window->setVerticalSyncEnabled(true);
    window->setFramerateLimit(0);
  } else {
    window->setVerticalSyncEnabled(false);
    window->setFramerateLimit(60);
  }
}

void GuiManager::quit() {
  quitting_ = true;
  if (gameRunner_ != 0) {
    gameRunner_->quit();
  }
}

void GuiManager::redrawMainWindow() {
  if (window_ != 0) {
    drawFrame(window_);
  }
}

void GuiManager::logErrorMessage(lua_State *L, const char *formatString) {
  const char *luaMessage = lua_tostring(L, -1);
  int messageLen = (int) (strlen(formatString) + strlen(luaMessage) - 2);
  char *errorMessage = new char[messageLen + 1];
  sprintf(errorMessage, formatString, luaMessage);
  errorConsole_->println(errorMessage);
  delete errorMessage;
  errorConsole_->Show();
  errorConsole_->Raise();
}

char* GuiManager::getStagesDirCopy() {
  char *stagesDir = new char[getStagesDir().length() + 1];
  strcpy(stagesDir, getStagesDir().c_str());
  return stagesDir;
}

char* GuiManager::getShipsDirCopy() {
  char *shipsDir = new char[getShipsDir().length() + 1];
  strcpy(shipsDir, getShipsDir().c_str());
  return shipsDir;
}

char* GuiManager::getCacheDirCopy() {
  char *cacheDir = new char[getCacheDir().length() + 1];
  strcpy(cacheDir, getCacheDir().c_str());
  return cacheDir;
}

char* GuiManager::getTmpDirCopy() {
  char *tmpDir = new char[getTmpDir().length() + 1];
  strcpy(tmpDir, getTmpDir().c_str());
  return tmpDir;
}

MatchStarter::MatchStarter(GuiManager *guiManager) {
  guiManager_ = guiManager;
}

void MatchStarter::startMatch(const char *stageName, char **teamNames,
                              int numTeams) {
  guiManager_->startMatch(stageName, teamNames, numTeams);
}

void MatchStarter::previewStage(const char *stageName) {
  guiManager_->showStagePreview(stageName);
}

void MatchStarter::refreshFiles() {
  guiManager_->loadStages();
  guiManager_->loadShips();
}

void MatchStarter::onClose() {
  guiManager_->dialogClosed();
}

void MatchStarter::onEscape() {
  guiManager_->dialogEscaped();
}

void MatchStarter::onUpdateUi() {
  guiManager_->redrawMainWindow();
}

void MatchStarter::reloadBaseDirs() {
  guiManager_->reloadBaseDirs();
  refreshFiles();
}

ShipPackager::ShipPackager(GuiManager *guiManager, FileManager *fileManager,
                           OutputConsole *packagingConsole) {
  packagingConsole_ = packagingConsole;
  guiManager_ = guiManager;
  fileManager_ = fileManager;
}

void ShipPackager::package(const char *shipName, const char *version,
                           bool obfuscate) {
  bool refresh = true;
  try {
    fileManager_->packageShip(getShipsDir().c_str(), shipName, version,
                              getCacheDir().c_str(), getTmpDir().c_str(),
                              obfuscate, false);
  } catch (FileExistsException *e) {
    std::stringstream overwriteStream;
    overwriteStream << "File already exists: " << e->what() << std::endl
                    << std::endl << "OK to overwrite it?";
    wxMessageDialog errorMessage(NULL, overwriteStream.str(), "Are you sure?",
                                 wxOK | wxCANCEL | wxICON_QUESTION);
    int r = errorMessage.ShowModal();
    if (r == wxID_OK) {
      fileManager_->packageShip(getShipsDir().c_str(), shipName, version,
                                getCacheDir().c_str(), getTmpDir().c_str(),
                                obfuscate, true);
      fileManager_->deleteFromCache(getCacheDir().c_str(), e->what());
    } else {
      refresh = false;
    }
    delete e;
  } catch (std::exception *e) {
    packagingConsole_->clear();
    packagingConsole_->Show();
    packagingConsole_->println("Packaging ship failed: ");
    packagingConsole_->print("  ");
    packagingConsole_->println(e->what());
    delete e;
  }

  if (refresh) {
    guiManager_->loadShips();
  }
}

void ShipPackager::refreshFiles() {
  guiManager_->loadStages();
  guiManager_->loadShips();
}

void ShipPackager::onClose() {
  guiManager_->dialogClosed();
}

void ShipPackager::onEscape() {
  guiManager_->dialogEscaped();
}

void ShipPackager::onUpdateUi() {
  guiManager_->redrawMainWindow();
}

StagePackager::StagePackager(GuiManager *guiManager, FileManager *fileManager,
    OutputConsole *packagingConsole) {
  packagingConsole_ = packagingConsole;
  guiManager_ = guiManager;
  fileManager_ = fileManager;
}

void StagePackager::package(const char *stageName, const char *version,
                            bool obfuscate) {
  bool refresh = true;
  try {
    fileManager_->packageStage(getStagesDir().c_str(), stageName, version,
                               getCacheDir().c_str(), getTmpDir().c_str(),
                               obfuscate, false);
  } catch (FileExistsException *e) {
    std::stringstream overwriteStream;
    overwriteStream << "File already exists: " << e->what() << std::endl
                    << std::endl << "OK to overwrite it?";
    wxMessageDialog errorMessage(NULL, overwriteStream.str(), "Are you sure?",
                                 wxOK | wxCANCEL | wxICON_QUESTION);
    int r = errorMessage.ShowModal();
    if (r == wxID_OK) {
      fileManager_->packageStage(getStagesDir().c_str(), stageName, version,
                                 getCacheDir().c_str(), getTmpDir().c_str(),
                                 obfuscate, true);
      fileManager_->deleteFromCache(getCacheDir().c_str(), e->what());
    } else {
      refresh = false;
    }
    delete e;
  } catch (std::exception *e) {
    packagingConsole_->clear();
    packagingConsole_->Show();
    packagingConsole_->println("Packaging stage failed: ");
    packagingConsole_->print("  ");
    packagingConsole_->println(e->what());
    refresh = false;
    delete e;
  }

  if (refresh) {
    guiManager_->loadStages();
  }
}

void StagePackager::refreshFiles() {
  guiManager_->loadStages();
  guiManager_->loadShips();
}

void StagePackager::onClose() {
  guiManager_->dialogClosed();
}

void StagePackager::onEscape() {
  guiManager_->dialogEscaped();
}

void StagePackager::onUpdateUi() {
  guiManager_->redrawMainWindow();
}

RunnerLauncher::RunnerLauncher(GuiManager *guiManager, FileManager *fileManager,
    OutputConsole *runnerConsole) {
  runnerConsole_ = runnerConsole;
  guiManager_ = guiManager;
  fileManager_ = fileManager;
}

void RunnerLauncher::launch(const char *runnerName) {
  guiManager_->launchGameRunner(runnerName);
}

void RunnerLauncher::refreshFiles() {
  guiManager_->loadRunners();
}

void RunnerLauncher::onClose() {
  guiManager_->dialogClosed();
}

void RunnerLauncher::onEscape() {
  guiManager_->dialogEscaped();
}

void RunnerLauncher::onUpdateUi() {
  guiManager_->redrawMainWindow();
}

PackageReporter::PackageReporter(OutputConsole *packagingConsole) {
  packagingConsole_ = packagingConsole;
}

void PackageReporter::packagingComplete(char **sourceFiles, int numFiles,
    bool obfuscate, const char *destinationFile) {
  packagingConsole_->clear();
  packagingConsole_->Show();
  if (obfuscate) {
    packagingConsole_->println(
        "The following files were packaged as obfuscated source code:");
  } else {
    packagingConsole_->println(
        "The following files were packaged as source code:");
  }
  for (int x = 0; x < numFiles; x++) {
    if (sourceFiles[x] != 0) {
      packagingConsole_->print("  ");
      packagingConsole_->println(sourceFiles[x]);
    }
  }
  packagingConsole_->println();
  packagingConsole_->print("Saved to: ");
  packagingConsole_->println(destinationFile);
  packagingConsole_->Raise();
}

StageConsoleListener::StageConsoleListener(Stage *stage) {
  stage_ = stage;
}

void StageConsoleListener::onCheck(bool checked) {
  stage_->setGfxEnabled(checked);
}

PreviewInputListener::PreviewInputListener(GuiManager *guiManager) {
  guiManager_ = guiManager;
}

void PreviewInputListener::onClose() {
  guiManager_->closeStagePreview();
}

void PreviewInputListener::onUp() {
  guiManager_->previewPreviousStage();
}

void PreviewInputListener::onDown() {
  guiManager_->previewNextStage();
}

RunnerConsoleListener::RunnerConsoleListener(GuiManager *guiManager) {
  guiManager_ = guiManager;
}

void RunnerConsoleListener::onClose() {
  guiManager_->abortGameRunner();
}

void RunnerConsoleListener::onAbort() {
  guiManager_->abortGameRunner();
}

ResultsRestarter::ResultsRestarter(GuiManager *guiManager) {
  guiManager_ = guiManager;
}

void ResultsRestarter::onRestart() {
  guiManager_->restartMatch();
}

RunnerFormRedrawer::RunnerFormRedrawer(GuiManager *guiManager) {
  guiManager_ = guiManager;
}

void RunnerFormRedrawer::onUpdateUi() {
  guiManager_->redrawMainWindow();
}

ViewListener::ViewListener(GuiManager *guiManager) {
  guiManager_ = guiManager;
}

void ViewListener::onNewMatch() {
  guiManager_->showNewMatchDialog();
}

void ViewListener::onPackageShip() {
  guiManager_->showPackageShipDialog();
}

void ViewListener::onPackageStage() {
  guiManager_->showPackageStageDialog();
}

void ViewListener::onStageClick() {
  guiManager_->showStageConsole();
}

void ViewListener::onTeamClick(int teamIndex) {
  guiManager_->showTeamConsole(teamIndex);
}

void ViewListener::onPauseUnpause() {
  guiManager_->togglePause();
}

void ViewListener::onRestart() {
  guiManager_->restartMatch();
}

void ViewListener::onTpsChange(double tpsFactor) {
  guiManager_->setTpsFactor(tpsFactor);
}
