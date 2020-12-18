#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Initialise variables
    dlMode = DLMODE_NONE;
    viewMode = VIEWMODE_NONE;
    proxyPort = 0;

    this->setWindowTitle(QString("Series-app %1").arg(SERIESAPP_VERSION));
    ui->stackedWidget->setCurrentIndex(STACKED_WIDGET_PAGE_MAIN);

    // Try to load settings file
    if (loadSettingsFile()) {
        ui->label->setText("Loaded settings file.");
    }

    setProxy();
    // Connect download manager signal
    connect(&manager, &QNetworkAccessManager::finished,
            this, &MainWindow::downloadFinished);

    // Load favourites from file
    loadFavListFile();

    // Retrieve series list
    ui->label->setText("Loading list of all series...");
    // First try to load series list from file
    if (!loadSeriesListFile()) {
        // That failed. Will have to download it
        on_actionRe_download_seriesList_triggered();
    } else {

        // Get how old seriesList.txt is in days
        currentListAge = calculateDaysOld(seriesListInfo);
        QString lbl = "List loaded from seriesList.txt";
        addDaysOldString(lbl, currentListAge);

        // Update user interface
        ui->lineEdit->clear();
        on_getButton_clicked();
        ui->label->setText(lbl);
        viewMode = VIEWMODE_SERIES;
        updateGUI();
    }

    // Set focus to search bar
    ui->lineEdit->setFocus();
}

MainWindow::~MainWindow()
{
    delete ui;
}

int MainWindow::calculateDaysOld(QFileInfo fileInfo)
{
    QDate today = QDate::currentDate();
    QDate last = fileInfo.lastModified().date();
    return last.daysTo(today);
}

void MainWindow::addDaysOldString(QString &str, int days)
{
    if (days >= 0) {
        str.append(" (");
        str.append(QString::number(days));
        if (days == 1) {
            str.append(" day old");
        } else {
            str.append(" days old");
        }
        str.append(")");
    }
}

void MainWindow::log(QString msg)
{
    ui->textBrowser_ErrorLog->append(msg + "\n");
}

/* Returns the settings directory, with the optional specified filename added to
 * the end. If the directory does not exist, it is created. */
QString MainWindow::getSettingsDir(QString addfile)
{
    // Settings dir is standard (XDG) config dir
    QString settingsDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);

    // Create dir if it doesn't exist
    QDir dir(settingsDir);
    if (!dir.exists()) {
        if (dir.mkpath(settingsDir)) {
            log("Created settings directory: " + settingsDir);
        } else {
            log("Failed to create settings directory: " + settingsDir);
        }
    }

    return settingsDir + "/" + addfile;
}

QString MainWindow::getSeriesCacheFilename(QString seriesTVRage, QString seriesDir)
{
    return "epscache_" + seriesTVRage + "_" + seriesDir + ".txt";
}

void MainWindow::doDownload(const QUrl &url)
{
    QNetworkRequest request(url);
    manager.get(request);
}

QString MainWindow::getSeriesDir(const QString &series)
{
    return series.split(",")[1]; // Series directory
}

QString MainWindow::getSeriesRage(const QString &series)
{
    return series.split(",")[2]; // Series Rage code
}

QString MainWindow::getSeriesMaze(const QString &series)
{
    return series.split(",")[3]; // Series Maze code
}

QString MainWindow::getSeriesName(const QString &series)
{
    QString name = series.split(",")[0]; // Series name
    // Remove quotes:
    name.remove(0,1);
    name.remove(name.count()-1,1);
    return name;
}

QString MainWindow::getEpName(const QString &episode)
{
    QStringList ep = episode.split(",");
    QString name = ep[4]; // Episode name
    // Remove quotes:
    name.remove(0,1);
    name.remove(name.count()-1,1);
    return name;
}

int MainWindow::strToMonth(QString month)
{
    int M;

    if (month == "Jan") M = 1;
    else if (month == "Feb") M = 2;
    else if (month == "Mar") M = 3;
    else if (month == "Apr") M = 4;
    else if (month == "May") M = 5;
    else if (month == "Jun") M = 6;
    else if (month == "Jul") M = 7;
    else if (month == "Aug") M = 8;
    else if (month == "Sep") M = 9;
    else if (month == "Oct") M = 10;
    else if (month == "Nov") M = 11;
    else if (month == "Dec") M = 12;
    else M = 1;

    return M;
}

// Extract episode date
epDateReturn MainWindow::getEpDate(const QString &episode, const QString &series)
{
    epDateReturn ret;
    ret.valid = false;

    QStringList ep = episode.split(",");
    if (ep.count()<4) {
        log("getEpDate: Not enough episode fields; " + episode);
        return ret;
    }
    QString date = ep[3]; // Episode date

    // Date string in the form "02 Dec 13"

    QStringList dlist = date.split(" ");
    if (dlist.count() != 3) {
        // Date format not valid
        log("getEpDate: Invalid date format; " + episode);
        return ret;
    }

    QString day = dlist[0];
    QString month = dlist[1];
    QString year = dlist[2];

    int D = day.toInt();
    int Y = year.toInt();
    int M = strToMonth(month);

    /* The year Y is now only a year number without a century
       Must now extract series start date,
       check its year (with century included).
       If it is before 2000, add 1900 to Y.
       If, Y is still smaller than the start year,
       it must have crossed the 2000 mark, so add 100.
       Otherwise if start date is after 2000,
       add 2000 to Y.
    */

    // Extract series start date to determine century
    QStringList sr = series.split(",");
    if (sr.count() < 5) {
        log("getEpDate: Not enough series fields; " + series);
        return ret;
    }
    QString sdate = sr[4]; // start date
    QStringList slist = sdate.split(" ");
    if (slist.count() < 2) {
        log("getEpDate: Series start date format invald; " + series);
        return ret;
    }
    QString syear = slist[1];
    int YS = syear.toInt();
    if (YS<2000) {
        Y += 1900;
        if (Y<YS) {
            Y += 100;
        }
    } else {
        Y += 2000;
    }

    QDate epdate = QDate(Y,M,D);
    epDateReturn r;
    r.date = epdate;
    r.valid = true;

    return r;
}

QString MainWindow::getEpNumber(const QString &episode)
{
    QStringList ep = episode.split(",");
    QString entry = ep[2]; // Episode nr
    if (entry.length() == 1) { entry.prepend("0"); }
    entry.prepend(ep[1]); // Series nr

    return entry;
}

void MainWindow::downloadFinished(QNetworkReply *reply)
{
    if (reply->error()) {
        ui->label->setText("Download failed");
    } else {

        if (dlMode == DLMODE_SERIESLIST) {

            ui->label->setText("Download finished, building series list...");

            // Clear all lists:
            seriesList.clear();
            seriesListGUI.clear();
            ui->listWidget->clear();

            while (!reply->atEnd()) {
                QString line = reply->readLine();
                addLineToSeriesList(line);
            }

            // Update user interface
            ui->lineEdit->clear();
            on_getButton_clicked();

            ui->label->setText("Series list downloaded from epguides.com");
            viewMode = VIEWMODE_SERIES;

            // Save series list so that we don't have to download it in the future.
            saveSeriesFile();

        } else if (dlMode == DLMODE_EPLIST) {

            ui->label->setText("Download finished, building episodes list...");

            QString line;

            // Clear lists
            clearEpisodeLists();

            while (!reply->atEnd()) {
                line = reply->readLine();
                addLineToEpisodeList(line);
            }

            ui->label->setText(getSeriesName(currentSeries));

            // Save episode list to cache file
            saveEpCacheFile();

        }
    }

    dlMode = DLMODE_NONE;
    updateGUI();
}

void MainWindow::clearEpisodeLists()
{
    ui->listWidget->clear(); // GUI list of episodes
    epList.clear();          // List of episode friendly names
    epLineList.clear();      // List of raw episode lines
}

/* Search button clicked */
void MainWindow::on_getButton_clicked()
{
    // Search for string in the list of series
    ui->listWidget->clear();
    seriesListGUI.clear();
    int i = 0; // Counter for index in seriesList
    QString item;
    while (i<seriesList.count()) {
        item = seriesList.value(i);
        if (item.toLower().contains(ui->lineEdit->text().toLower())) {
            // Add name to GUI list
            ui->listWidget->addItem(getSeriesName(item));
            // Add series index to parallel list
            seriesListGUI.append(i);
        }
        i++;
    }
    viewMode = VIEWMODE_SERIES;
    ui->label->setText("Series:");

    // If search is empty, add favourites on the top of the list
    if (ui->lineEdit->text().length()==0) {
        addFavListToGUI();
    }

    // If only one series is in the list, go directly to it.
    if (ui->listWidget->count() == 1) {
        loadEpList(0);
    }
}

void MainWindow::addFavListToGUI()
{
    int i=0;
    for (i=favList.count()-1; i>=0; i--) {
        // Add name to GUI list
        ui->listWidget->insertItem(0,getSeriesName(favList[i]));
        QColor kleur = QColor(200,200,20);
        ui->listWidget->item(0)->setBackgroundColor(kleur);
        // Add series index to parallel list
        seriesListGUI.prepend(-1);
    }
}

void MainWindow::on_lineEdit_returnPressed()
{
    on_getButton_clicked();
}

void MainWindow::loadEpList(int index)
{
    // Use index to get a series in seriesListGUI, then use that index to get
    // the series from the main seriesList.
    // If the entry in seriesListGUI is negative, it is a favourite.
    if (seriesListGUI[index] >= 0) {
        loadEpList(seriesList[seriesListGUI[index]]);
        currentEpIsFavourite = 0;
    } else {
        // Negative: load from favourite list
        loadEpList(favList[index]);
        currentEpIsFavourite = 1;
    }
    updateGUI();
}

void MainWindow::loadEpList(QString ep, bool redownload)
{
    currentSeries = ep;
    //QString rage = getSeriesRage(currentSeries);
    QString maze = getSeriesMaze(currentSeries);
    QString dir = getSeriesDir(currentSeries);
    viewMode = VIEWMODE_EPISODES;

    clearEpisodeLists();

    // Try to load from cache file first
    //if (loadEpListFile(rage, dir) && !redownload) {
    if (loadEpListFile(maze, dir) && !redownload) {

        // Get how old file is in days
        currentListAge = calculateDaysOld(epListFileInfo);
        QString lbl = "Episode list loaded from cache file";
        addDaysOldString(lbl, currentListAge);

        ui->notifyLabel->setText(lbl);
        ui->label->setText(getSeriesName(currentSeries));

    } else {
        // Could not load from cache file. Start a download

        if (maze.isEmpty()) {
            log("loadEpList: Series maze number empty; " + currentSeries);
            ui->label->setText("Series has no maze number.");
        } else {

            //QString address = "http://epguides.com/common/exportToCSV.asp?rage=";
            QString address = "http://epguides.com/common/exportToCSVmaze.asp?maze=";

            //address.append( rage );
            address.append( maze );

            QUrl url = QUrl(address);
            dlMode = DLMODE_EPLIST;
            ui->label->setText("Downloading episode list...");

            doDownload(url);
        }
    }
}

void MainWindow::on_listWidget_doubleClicked(QModelIndex index)
{
    if (viewMode == VIEWMODE_SERIES) {
        loadEpList(index.row());
    } else if (viewMode == VIEWMODE_EPISODES) {

        // Copy episode name to clipboard
        QClipboard *clipboard = QApplication::clipboard();

        clipboard->setText(epList[index.row()]);
        ui->notifyLabel->setText("'" + epList[index.row()] + "' copied to clipboard.");

    }
}

// Loads series list from file, and if it fails returns false.
bool MainWindow::loadSeriesListFile()
{
    QFile file(getSettingsDir(SERIESLIST_FILENAME));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return false; // Failed to open file
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        addLineToSeriesList(line);
    }

    seriesListInfo = QFileInfo(file);
    return true;
}

// Loads favourites list from file, and if it fails returns false.
bool MainWindow::loadFavListFile()
{
    QFile file(getSettingsDir(SERIESLIST_FAV_FILENAME));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return false; // Failed to open file
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.length()>1) {
            favList.append(line);
        }
    }

    return true;
}

// Loads cached episode list from file, returns false if file doesn't exist
bool MainWindow::loadEpListFile(QString seriesTVRage, QString seriesDir)
{
    // First check if file epscache_<tvrage>_<seriesdir>.txt exists
    QFile file(getSettingsDir(getSeriesCacheFilename(seriesTVRage, seriesDir)));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // File does not exist.
        return false;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        addLineToEpisodeList(line);
    }

    epListFileInfo = QFileInfo(file);
    return true;
}

// Adds a line that is read from the series list text file to the seriesList(s)
void MainWindow::addLineToSeriesList(QString line)
{
    if (line[0] == '"') {
        // Append the whole series line to the main seriesList
        seriesList.append(line);
        // Add the series name to the GUI list
        //ui->listWidget->addItem(getSeriesName(&line));
        // Also keep a parallel list with all the series Rage numbers
        //seriesListGUI.append(seriesList.count()-1);
    }
}

// Parse an episode line and add to the appropriate lists
void MainWindow::addLineToEpisodeList(QString line)
{
    QString entry;
    QString epname;

    if (line[0].isDigit()) {

        // Build GUI list entry
        entry = getEpNumber(line); // Episode number
        entry.append("   ");
        entry.append(getEpName(line)); // Episode naam
        ui->listWidget->insertItem(0,entry); // Insert at top of list
        epDateReturn dateReturn = getEpDate(line, currentSeries);
        if (dateReturn.valid) {
            ui->listWidget->item(0)->setToolTip(dateReturn.date.toString());
        }

        // Build episode friendly name list entry (in format eg. "Dexter 101 - Pilot")
        epname = getSeriesName(currentSeries);
        epname.append(" ");
        epname.append(getEpNumber(line));
        epname.append(" - ");
        epname.append(getEpName(line));
        epList.prepend(epname);

        if (dateReturn.valid) {
            // Check if episode is released yet and grey out background if not
            if (dateReturn.date.operator >(dateReturn.date.currentDate())) {
                QColor kleur = QColor(150,150,150);
                ui->listWidget->item(0)->setBackgroundColor(kleur);

            }
        }

        // Build episode line list (for saving to a file later)
        epLineList.append(line);

    }
}

void MainWindow::saveSeriesFile()
{
    QFile file(getSettingsDir(SERIESLIST_FILENAME));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        ui->label->setText("Could not save list to disk");
        return;
    }

    QTextStream out(&file);
    int i = 0;
    while (i<seriesList.count()) {
        out << seriesList[i];
        i++;
    }
    ui->label->setText("Saved list to disk");
    file.close();
}

void MainWindow::saveEpCacheFile()
{
    QString filename = getSeriesCacheFilename(getSeriesMaze(currentSeries),
                                              getSeriesDir(currentSeries));
    QFile file(getSettingsDir(filename));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        ui->notifyLabel->setText("Could not save episode list cache file");
        return;
    }

    QTextStream out(&file);
    int i = 0;
    while (i<epLineList.count()) {
        out << epLineList[i];
        i++;
    }
    ui->notifyLabel->setText("Saved list to cache file");
    file.close();
}

void MainWindow::saveFavFile()
{
    QFile file(getSettingsDir(SERIESLIST_FAV_FILENAME));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        ui->label->setText("Error writing to favourites file");
        return;
    }

    QTextStream out(&file);
    int i = 0;
    while (i<favList.count()) {
        out << favList[i] << "\n";
        i++;
    }

    file.close();
}

void MainWindow::saveSettingsFile()
{
    QFile file(getSettingsDir(SETTINGS_FILENAME));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        ui->label->setText("Error writing settings file");
        return;
    }

    QTextStream out(&file);
    out << SETTINGS_PROXY_ADDRESS << " " << proxyAddress << "\n";
    out << SETTINGS_PROXY_PORT << " " << QString::number(proxyPort) << "\n";

    file.close();
    ui->label->setText("Saved settings file.");
}

bool MainWindow::loadSettingsFile()
{
    QFile file(getSettingsDir(SETTINGS_FILENAME));
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false; // Could not open file
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        QStringList words = line.split(" ");
        if (words.count() > 1) {
            if (words[0] == SETTINGS_PROXY_ADDRESS) {
                proxyAddress = words[1];
            } else if (words[0] == SETTINGS_PROXY_PORT) {
                proxyPort = words[1].toInt();
            }
        }
    }

    file.close();

    return true;
}

void MainWindow::on_listWidget_clicked(const QModelIndex& /*index*/)
{
    updateGUI();
}

void MainWindow::toggleStarButton(int bright)
{
    if (bright) {
        ui->starButton->setIcon(QIcon(":star_bright"));
    } else {
        ui->starButton->setIcon(QIcon(":star_dim"));
    }
}

void MainWindow::updateGUI()
{
    if (viewMode == VIEWMODE_SERIES) {

        currentEpIsFavourite = 0;
        // Update Star Button
        if (ui->listWidget->selectedItems().count()) {

            ui->starButton->setEnabled(1);

            toggleStarButton(0); // Make dim

            // Check if favourite item is selected
            int i;
            for (i=0; i<favList.length(); i++) {
                if (ui->listWidget->count() < favList.length()) { break; }
                if (ui->listWidget->item(i)->isSelected()) {
                    if (seriesListGUI[i] < 0) {
                        // This means a favourite item is selected
                        toggleStarButton(1); // Make bright
                        break;
                    }
                }
            }

        } else {
            ui->starButton->setEnabled(0);
        }


        // Update Refresh button
        QString refreshTooltip = "Re-download series list";
        addDaysOldString(refreshTooltip, currentListAge);
        ui->refreshButton->setToolTip(refreshTooltip);

        // Update Back button
        ui->backButton->setToolTip("Clear Search");

    } else if (viewMode == VIEWMODE_EPISODES) {

        // Update Star Button
        ui->starButton->setEnabled(true);
        if (currentEpIsFavourite) {
            toggleStarButton(1); // make bright
        } else {
            toggleStarButton(0); // make dim
        }

        // Update refresh button
        QString refreshTooltip = "Re-download series list";
        addDaysOldString(refreshTooltip, currentListAge);
        ui->refreshButton->setToolTip(refreshTooltip);

        // Update back button
        ui->backButton->setToolTip("Back to series list");
    }
}

void MainWindow::on_actionRe_download_seriesList_triggered()
{
    ui->label->setText("Downloading list of all series...");
    dlMode = DLMODE_SERIESLIST;
    QUrl url = QUrl("http://epguides.com/common/allshows.txt");
    doDownload(url);
}

void MainWindow::on_actionAdd_to_favourites_triggered()
{
    if (viewMode==VIEWMODE_SERIES) {
        // Add selected item to favList
        int i;
        for (i=0; i < ui->listWidget->count(); i++) {
            if (ui->listWidget->item(i)->isSelected()) {
                if (seriesListGUI[i] < 0) {
                    // This means item is in favourites list
                    favList.removeAt(i);
                    on_getButton_clicked(); // refresh listWidget
                    break;
                } else {
                    favList.append(seriesList[seriesListGUI[i]]);
                    on_getButton_clicked(); // refresh listWidget
                }
            }
        }

    } else if (viewMode==VIEWMODE_EPISODES) {

        if (currentEpIsFavourite==0) {
            // Add currentSeries to favList
            favList.append(currentSeries);
            currentEpIsFavourite = 1;
            updateGUI();
        }
    }

    // Save favlist to file
    saveFavFile();
}

void MainWindow::on_actionBack_triggered()
{
    ui->lineEdit->clear();
    on_getButton_clicked();
}

void MainWindow::on_actionClear_Search_triggered()
{
    ui->lineEdit->clear();
    on_getButton_clicked();
}

void MainWindow::on_actionRe_download_episode_list_triggered()
{
    loadEpList(currentSeries,true);
}

void MainWindow::on_backButton_clicked()
{
    on_actionBack_triggered();
}

void MainWindow::on_starButton_clicked()
{
    on_actionAdd_to_favourites_triggered();
}

void MainWindow::on_refreshButton_clicked()
{
    if ( (viewMode==VIEWMODE_SERIES) || (viewMode==VIEWMODE_NONE) ) {
        on_actionRe_download_seriesList_triggered();
    } else if (viewMode==VIEWMODE_EPISODES) {
        on_actionRe_download_episode_list_triggered();
    }
}

void MainWindow::on_pushButton_SettingsOK_clicked()
{
    proxyAddress = ui->lineEdit_ProxyAddress->text();
    proxyPort = ui->lineEdit_ProxyPort->text().toInt();

    saveSettingsFile();

    setProxy();

    ui->stackedWidget->setCurrentIndex(STACKED_WIDGET_PAGE_MAIN);
}

void MainWindow::setProxy()
{
    QNetworkProxy proxy;

    if (proxyAddress.isEmpty()) {
        proxy.setType(QNetworkProxy::NoProxy);
    } else {

        proxy.setType(QNetworkProxy::HttpProxy);

        proxy.setHostName( proxyAddress );
        proxy.setPort( proxyPort );
    }

    manager.setProxy(proxy);
}

void MainWindow::on_pushButton_SettingsCancel_clicked()
{
    ui->stackedWidget->setCurrentIndex(STACKED_WIDGET_PAGE_MAIN);
}

void MainWindow::on_settingsButton_clicked()
{
    ui->lineEdit_ProxyAddress->setText( proxyAddress );
    ui->lineEdit_ProxyPort->setText( QString::number(proxyPort) );

    ui->stackedWidget->setCurrentIndex(STACKED_WIDGET_PAGE_SETTINGS);
}
