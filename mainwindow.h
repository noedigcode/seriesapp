#ifndef SERIES_H
#define SERIES_H


#include <QClipboard>
#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QListWidgetItem>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QStringList>
#include <QUrl>
#include <QWidget>

#define STACKED_WIDGET_PAGE_MAIN 0
#define STACKED_WIDGET_PAGE_SETTINGS 1

#define SETTINGS_FILENAME "seriesSettings.txt"
#define SETTINGS_PROXY_ADDRESS "proxyAddress"
#define SETTINGS_PROXY_PORT "proxyPort"

#define SERIESLIST_FILENAME "seriesList.txt"
#define SERIESLIST_FAV_FILENAME "seriesListFavourites.txt"

#define SERIESAPP_VERSION "1.1.0" // The program version

#define DLMODE_NONE "none"
#define DLMODE_SERIESLIST "seriesList"
#define DLMODE_EPLIST "epList"

#define VIEWMODE_NONE "none"
#define VIEWMODE_SERIES "series"
#define VIEWMODE_EPISODES "episodes"

struct epDateReturn
{
    bool valid;
    QDate date;
};

namespace Ui {
    class MainWindow;
}

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    QNetworkAccessManager manager;  // Object that manages downloads
    QNetworkReply *currentDownload;

    QStringList seriesList;     // List of all series
    QList<int> seriesListGUI;   // List of all series displayed in GUI,
                                // containing int referring to index of series in main seriesList
    QStringList favList;        // Favourite series list
    QStringList epList;         // Contains friendly names (series ep# - epname) of episodes currently being displayed
    QStringList epLineList;     // Contains episode list lines (raw)
    QString dlMode;             // Mode of current download
    QString viewMode;           // What the list is currently viewing; one of: series, episodes
    QString currentSeries;      // Current series being viewed
    int selectedIndex;          // Index of series currently selected in the listWidget
    QFileInfo seriesListInfo;   // Info of the seriesList file
    QFileInfo epListFileInfo;   // Info of the cached episodes file

    int currentListAge;         // Age of cache file of currently displayed list, in days
    int currentEpIsFavourite;   // Indicates whether the current episode is in the favourite list or not

    int calculateDaysOld(QFileInfo fileInfo);
    void addDaysOldString(QString &str, int days);

    QString proxyAddress;
    int proxyPort;
    void setProxy();

    void log(QString msg);

    QString getSettingsDir(QString addfile = "");
    QString getSeriesCacheFilename(QString seriesTVRage, QString seriesDir);
    void doDownload(const QUrl &url);

    QString getEpName(const QString &episode);
    QString getSeriesName(const QString &MainWindow);
    QString getSeriesRage(const QString &MainWindow);
    QString getSeriesMaze(const QString &MainWindow);
    QString getSeriesDir(const QString &MainWindow);
    QString getEpNumber(const QString &episode);
    epDateReturn getEpDate(const QString &episode, const QString &MainWindow);

    void loadEpList(int index);
    void loadEpList(QString ep, bool redownload = false);
    void addLineToSeriesList(QString line);
    void saveSeriesFile();
    bool loadSeriesListFile();
    int strToMonth(QString month);
    void addFavListToGUI();
    bool loadFavListFile();
    void saveFavFile();
    bool loadSettingsFile();
    void saveSettingsFile();
    void clearEpisodeLists();
    bool loadEpListFile(QString seriesTVRage, QString seriesDir);
    void addLineToEpisodeList(QString line);
    void saveEpCacheFile();
    void updateGUI();
    void toggleStarButton(int bright);

private:
    Ui::MainWindow *ui;

private slots:
    void on_listWidget_doubleClicked(QModelIndex index);
    void on_lineEdit_returnPressed();
    void on_getButton_clicked();
    void downloadFinished(QNetworkReply *reply);
    void on_listWidget_clicked(const QModelIndex &index);
    void on_actionRe_download_seriesList_triggered();
    void on_actionAdd_to_favourites_triggered();
    void on_actionBack_triggered();
    void on_actionClear_Search_triggered();
    void on_actionRe_download_episode_list_triggered();
    void on_backButton_clicked();
    void on_starButton_clicked();
    void on_refreshButton_clicked();
    void on_pushButton_SettingsOK_clicked();
    void on_pushButton_SettingsCancel_clicked();
    void on_settingsButton_clicked();
};

#endif // SERIES_H
