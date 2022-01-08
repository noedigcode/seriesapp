#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDesktopServices>

MainWindow::MainWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Version in window title
    this->setWindowTitle(QString("Series-app %1").arg(SERIESAPP_VERSION));

    // Version in about dialog
    QString html = ui->textBrowser_about->toHtml();
    html.replace("%VERSION%", SERIESAPP_VERSION);
    ui->textBrowser_about->setHtml(html);

    ui->stackedWidget->setCurrentWidget(ui->page_main);

    ui->pushButton_OpenSettingsFolder->setToolTip(getSettingsDir());

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
    ui->textBrowser_ErrorLog->append(msg);
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

QString MainWindow::getSeriesCacheFilename(SeriesPtr s)
{
    return QString("epscache_%1_%2_%3.txt")
            .arg(s->mazeNo).arg(s->rageNo).arg(s->directory);
}

void MainWindow::doDownload(const QUrl &url)
{
    QNetworkRequest request(url);
    manager.get(request);
}

void MainWindow::downloadFinished(QNetworkReply *reply)
{
    if (reply->error()) {
        ui->label->setText("Download failed");
        log("Download failed of: " + reply->request().url().toString());
        log("Error: " + reply->errorString());

    } else {

        if (dlMode == DLMODE_SERIESLIST) {

            ui->label->setText("Download finished, building series list...");

            // Clear all lists:
            seriesList.clear();
            seriesNumberMap.clear();
            seriesListGUI.clear();
            ui->listWidget->clear();

            while (!reply->atEnd()) {
                addLineToSeriesList(reply->readLine());
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

            if (currentSeries) {
                ui->label->setText(currentSeries->name);
            }

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
    epList.clear();
}

/* Search button clicked */
void MainWindow::on_getButton_clicked()
{
    // Search for string in the list of series
    ui->listWidget->clear();
    seriesListGUI.clear();

    QString searchText = ui->lineEdit->text().toLower();
    for (int i=0; i < seriesList.count(); i++) {
        SeriesPtr s = seriesList[i];
        if (searchText.isEmpty() || s->name.toLower().contains(searchText)) {
            ui->listWidget->addItem(s->name);
            seriesListGUI.append(i);
        }
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
    for (int i=favList.count()-1; i >= 0; i--) {
        // Add name to GUI list
        QListWidgetItem* item = new QListWidgetItem();
        item->setText(favList[i]->name);
        item->setBackground(favBgColor);
        item->setForeground(favFgColor);
        ui->listWidget->insertItem(0, item);
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

void MainWindow::loadEpList(SeriesPtr s, bool redownload)
{
    currentSeries = s;
    viewMode = VIEWMODE_EPISODES;

    clearEpisodeLists();

    // Try to load from cache file first
    if (loadEpListFile(s) && !redownload) {

        // Get how old file is in days
        currentListAge = calculateDaysOld(epListFileInfo);
        QString lbl = "Episode list loaded from cache file";
        addDaysOldString(lbl, currentListAge);

        ui->notifyLabel->setText(lbl);
        ui->label->setText(s->name);

    } else {
        // Could not load from cache file. Start a download

        QString address;
        if (!s->mazeNo.isEmpty()) {
            address = QString("https://epguides.com/common/exportToCSVmaze.asp?maze=%1")
                    .arg(s->mazeNo);
        } else if (!s->rageNo.isEmpty()) {
            address = QString("https://epguides.com/common/exportToCSV.asp?rage=%1")
                    .arg(s->rageNo);
        }

        if (address.isEmpty()) {
            log("loadEpList: Series maze and rage numbers empty; " + currentSeries->rawText);
            ui->label->setText("Series has no maze or rage number.");
        } else {

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

        EpisodePtr ep = epList.value(index.row());
        if (ep) {
            QString text = QString("%1 %2 - %3")
                    .arg(ep->series->name).arg(ep->number).arg(ep->name);
            QApplication::clipboard()->setText(text);
            ui->notifyLabel->setText("'" + text + "' copied to clipboard.");
        }
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
        SeriesPtr s(new Series(line));
        if (s->valid) {
            favList.append(s);
        }
    }

    return true;
}

// Loads cached episode list from file, returns false if file doesn't exist
bool MainWindow::loadEpListFile(SeriesPtr s)
{
    // First check if file epscache_<tvrage>_<seriesdir>.txt exists
    QFile file(getSettingsDir(getSeriesCacheFilename(s)));
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

/* Adds a line that is read from the series list text file to the seriesList
 * if valid. */
void MainWindow::addLineToSeriesList(QString line)
{
    SeriesPtr s(new Series(line));
    if (s->valid) {
        QString no = QString("%1_%2").arg(s->mazeNo).arg(s->rageNo);
        if (!seriesNumberMap.contains(no)) {
            seriesList.append(s);
            seriesNumberMap.insert(no, s);
        } else {
            //log("Debug: duplicate series: " + s->name);
        }
    }
}

// Parse an episode line and add to the appropriate lists
void MainWindow::addLineToEpisodeList(QString line)
{
    EpisodePtr ep(new Episode(line, currentSeries));

    if (!ep->valid) {
        log("Episode line not valid: " + ep->rawText);
        return;
    }

    // Insert entry at top of list
    ui->listWidget->insertItem(0, QString("%1   %2")
                               .arg(ep->number).arg(ep->name));
    if (ep->date.isValid()) {
        // Date tooltip
        ui->listWidget->item(0)->setToolTip(ep->date.toString());

        // Check if episode is released yet and grey out background if not

        if (ep->date.operator >(QDate::currentDate())) {
            ui->listWidget->item(0)->setBackground(unreleasedBgColor);
            ui->listWidget->item(0)->setForeground(unreleasedFgColor);
        }
    }

    epList.prepend(ep);
}

void MainWindow::saveSeriesFile()
{
    QFile file(getSettingsDir(SERIESLIST_FILENAME));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        ui->label->setText("Could not save list to disk");
        return;
    }

    QTextStream out(&file);
    foreach (SeriesPtr s, seriesList) {
        out << s->rawText;
    }

    ui->label->setText("Saved list to disk");
    file.close();
}

void MainWindow::saveEpCacheFile()
{
    QString filename = getSeriesCacheFilename(currentSeries);
    QFile file(getSettingsDir(filename));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        ui->notifyLabel->setText("Could not save episode list cache file");
        return;
    }

    QTextStream out(&file);
    for (int i=epList.count() - 1; i >= 0; i--) {
        out << epList[i]->rawText;
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
    foreach (SeriesPtr s, favList) {
        out << s->rawText << "\n";
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
    out << SETTINGS_PROXY_SYSTEM << " " << QVariant(useSystemProxy).toString() << "\n";
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
            } else if (words[0] == SETTINGS_PROXY_SYSTEM) {
                useSystemProxy = QVariant(words[1]).toBool();
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
        ui->starButton->setIcon(QIcon("://res/icons/icons8-star-filled-48.png"));
    } else {
        ui->starButton->setIcon(QIcon("://res/icons/icons8-star-filled-dark-48.png"));
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
    QUrl url = QUrl("https://epguides.com/common/allshows.txt");
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
    useSystemProxy = ui->checkBox_proxySystem->isChecked();
    proxyAddress = ui->lineEdit_ProxyAddress->text();
    proxyPort = ui->lineEdit_ProxyPort->text().toInt();

    saveSettingsFile();

    setProxy();

    ui->stackedWidget->setCurrentWidget(ui->page_main);
}

void MainWindow::setProxy()
{
    QNetworkProxy proxy = QNetworkProxy::applicationProxy();

    if (useSystemProxy) {
        //QNetworkProxyFactory::setUseSystemConfiguration(true);
        //proxy = QNetworkProxyFactory::proxyForQuery(QNetworkProxyQuery(QUrl("https://epguides.com"))).value(0);
        proxy.setType(QNetworkProxy::DefaultProxy);
        log("Using system proxy settings.");
    } else if (proxyAddress.isEmpty()) {
        proxy.setType(QNetworkProxy::NoProxy);
        log("Using no proxy.");
    } else {
        proxy.setType(QNetworkProxy::HttpProxy);
        proxy.setHostName(proxyAddress);
        proxy.setPort(proxyPort);
        log(QString("Using proxy settings: %1:%2")
            .arg(proxyAddress).arg(proxyPort));
    }

    manager.setProxy(proxy);
}

void MainWindow::on_settingsButton_clicked()
{
    ui->checkBox_proxySystem->setChecked(useSystemProxy);
    ui->lineEdit_ProxyAddress->setText( proxyAddress );
    ui->lineEdit_ProxyPort->setText( QString::number(proxyPort) );

    ui->stackedWidget->setCurrentWidget(ui->page_settings);
}

void MainWindow::on_pushButton_OpenSettingsFolder_clicked()
{
    QUrl url = QUrl::fromLocalFile(getSettingsDir());
    QDesktopServices::openUrl(url);
}

void MainWindow::on_pushButton_About_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->page_about);
}

void MainWindow::on_button_settings_back_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->page_main);
}

void MainWindow::on_button_about_back_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->page_settings);
}
