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
#include <QSharedPointer>

#define STACKED_WIDGET_PAGE_MAIN 0
#define STACKED_WIDGET_PAGE_SETTINGS 1

#define SETTINGS_FILENAME "seriesSettings.txt"
#define SETTINGS_PROXY_ADDRESS "proxyAddress"
#define SETTINGS_PROXY_PORT "proxyPort"
#define SETTINGS_PROXY_SYSTEM "proxyUseSystem"

#define SERIESLIST_FILENAME "seriesList.txt"
#define SERIESLIST_FAV_FILENAME "seriesListFavourites.txt"

#define SERIESAPP_VERSION "1.1.1-testing" // The program version

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

struct Series
{
    Series(QString txt)
    {
        rawText = txt;
        if (!txt.startsWith('"')) {
            valid = false;
            return;
        }
        valid = true;

        QStringList cols = rawTextToColumns(txt);

        // Series name
        name = cols.value(0);
        // Remove quotes
        name.remove(0, 1);
        name.remove(name.count()-1, 1);

        // Series Rage code
        rageNo = cols.value(2);

        // Series Maze code
        mazeNo = cols.value(3);

        // Series directory
        directory = cols.value(1);

        // Series start date
        date = cols.value(4);
        year = date.split(" ").value(1).toInt();
    }

    bool valid;
    QString rawText;
    QString name;
    QString rageNo;
    QString mazeNo;
    QString directory;
    QString date;
    int year;

    static QStringList rawTextToColumns(QString txt)
    {
        QStringList cols;

        QString col;
        bool inQuotes = false;
        for (int i=0; i < txt.count(); i++) {
            QChar c = txt.at(i);
            if (c == '"') {
                inQuotes = !inQuotes;
                col += c;
            } else if (c == ',') {
                if (inQuotes) {
                    col += c;
                } else {
                    cols.append(col);
                    col.clear();
                    inQuotes = false;
                }
            } else {
                col += c;
            }
        }
        cols.append(col);

        return cols;
    }
};
typedef QSharedPointer<Series> SeriesPtr;

struct Episode
{
    Episode(QString txt, SeriesPtr parentSeries)
    {
        rawText = txt;
        series = parentSeries;

        if (txt.at(0).isDigit() || txt.startsWith("S")) {
            valid = true;
        } else {
            valid = false;
            return;
        }

        // Format if maze number was used:
        // number,season,episode,airdate,title,tvmaze link
        // 1,1,1,01 Oct 06,"Dexter","https://www.tvmaze.com/episodes/11596/dexter-1x01-dexter"

        // Format if rage number was used:
        // number,season,episode,production code,airdate,title,special?,tvrage
        // 1,1,1,"",9/Jun/89,"Pilot",n

        QStringList cols = Series::rawTextToColumns(txt);
        bool maze = !parentSeries->mazeNo.isEmpty();

        // Episode name
        name = cols.value( maze ? 4 : 5 );
        // Remove quotes
        name.remove(0, 1);
        name.remove(name.count()-1, 1);

        // Episode number
        number = cols.value(2);
        if (number.length() == 1) { number.prepend("0"); }
        number.prepend(cols.value(1)); // Season number
        if (cols.value(0).startsWith("S")) {
            number.prepend("S");
        }

        // Episode date
        QString dateRaw = cols.value( maze ? 3 : 4);
        if (maze) {
            date = QLocale(QLocale::English, QLocale::UnitedStates).toDate(
                       dateRaw, "dd MMM yy");
        } else {
            date = QLocale(QLocale::English, QLocale::UnitedStates).toDate(
                       dateRaw, "d/MMM/yy");
            if (!date.isValid()) {
                date = QLocale(QLocale::English, QLocale::UnitedStates).toDate(
                           dateRaw, "dd/MMM/yy");
            }
        }
        if (date.year() < series->year) {
            date = date.addYears(100);
        }
    }

    bool valid;
    SeriesPtr series;
    QString rawText;
    QString name;
    QString number;
    QDate date;
};
typedef QSharedPointer<Episode> EpisodePtr;

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

    QList<SeriesPtr> seriesList;  // List of all series
    QList<int> seriesListGUI;   // List of all series displayed in GUI,
                                // containing int referring to index of series in main seriesList
    QList<SeriesPtr> favList;        // Favourite series list
    QList<EpisodePtr> epList;
    QStringList epLineList;     // Contains episode list lines (raw)
    QString dlMode = DLMODE_NONE; // Mode of current download
    QString viewMode = VIEWMODE_NONE; // What the list is currently viewing; one of: series, episodes
    SeriesPtr currentSeries;      // Current series being viewed
    int selectedIndex;          // Index of series currently selected in the listWidget
    QFileInfo seriesListInfo;   // Info of the seriesList file
    QFileInfo epListFileInfo;   // Info of the cached episodes file

    int currentListAge;         // Age of cache file of currently displayed list, in days
    int currentEpIsFavourite;   // Indicates whether the current episode is in the favourite list or not

    int calculateDaysOld(QFileInfo fileInfo);
    void addDaysOldString(QString &str, int days);

    bool useSystemProxy = true;
    QString proxyAddress;
    int proxyPort = 0;
    void setProxy();

    void log(QString msg);

    QString getSettingsDir(QString addfile = "");
    QString getSeriesCacheFilename(SeriesPtr s);
    void doDownload(const QUrl &url);

    void loadEpList(int index);
    void loadEpList(SeriesPtr s, bool redownload = false);
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
    bool loadEpListFile(SeriesPtr s);
    void addLineToEpisodeList(QString line);
    void saveEpCacheFile();
    void updateGUI();
    void toggleStarButton(int bright);

private:
    Ui::MainWindow *ui;

    const QColor favBgColor {200, 200, 20};
    const QColor favFgColor {0, 0, 0};
    const QColor unreleasedBgColor {150, 150, 150};
    const QColor unreleasedFgColor {0, 0, 0};

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
    void on_pushButton_OpenSettingsFolder_clicked();
};

#endif // SERIES_H
