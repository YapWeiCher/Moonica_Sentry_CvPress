#include "Moonica_qt.h"

void Moonica_qt::runDashboard()
{
    QString csvFolder = "C:/Moonica/Projects/CVPress_Test/Report/";

    QDir dir(csvFolder);
    QStringList filters;
    filters << "*.csv";

    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files | QDir::NoSymLinks, QDir::Time);

    QVector<CleaningDashboardRecord> allRecords;

    for (const QFileInfo& fi : fileList)
    {
        QVector<CleaningDashboardRecord> fileRecords = readCleaningCsv(fi.absoluteFilePath());
        allRecords += fileRecords;
    }

    if (allRecords.isEmpty())
    {
        qDebug() << "No CSV records found in folder:" << csvFolder;
        return;
    }

    // optional: sort by time
    std::sort(allRecords.begin(), allRecords.end(),
        [](const CleaningDashboardRecord& a, const CleaningDashboardRecord& b)
        {
            return a.towerTriggeringTime < b.towerTriggeringTime;
        });

    // clear old layout/widgets
    if (ui.frame_dashboard->layout())
    {
        QLayout* oldLayout = ui.frame_dashboard->layout();
        QLayoutItem* item;
        while ((item = oldLayout->takeAt(0)) != nullptr)
        {
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }
        delete oldLayout;
    }

    QVBoxLayout* layout = new QVBoxLayout(ui.frame_dashboard);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(10);

    QChartView* triggerChart = createDurationChart(
        "Trigger To Start Cleaning Duration",
        allRecords,
        [](const CleaningDashboardRecord& r) { return r.triggerToStartCleaningDuration; });

    QChartView* cleaningChart = createDurationChart(
        "Cleaning Duration",
        allRecords,
        [](const CleaningDashboardRecord& r) { return r.cleaningDuration; });

    QChartView* totalChart = createDurationChart(
        "Total Duration",
        allRecords,
        [](const CleaningDashboardRecord& r) { return r.totalDuration; });

    layout->addWidget(triggerChart);
    layout->addWidget(cleaningChart);
    layout->addWidget(totalChart);

    ui.frame_dashboard->setLayout(layout);
}

QVector<CleaningDashboardRecord> Moonica_qt::readCleaningCsv(const QString& filePath)
{
    QVector<CleaningDashboardRecord> records;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return records;

    QTextStream in(&file);

    if (in.atEnd())
        return records;

    // Read header
    QString headerLine = in.readLine().trimmed();
    QStringList headers = headerLine.split(",");

    int idxId = headers.indexOf("ID");
    int idxTowerTriggeringTime = headers.indexOf("TowerTriggeringTime");
    int idxTriggerToStart = headers.indexOf("TriggerToStartCleaningDuration");
    int idxCleaningDuration = headers.indexOf("CleaningDuration");
    int idxTotalDuration = headers.indexOf("TotalDuration");

    while (!in.atEnd())
    {
        QString line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;

        QStringList cols = line.split(",");

        int needSize = std::max({ idxId, idxTowerTriggeringTime, idxTriggerToStart, idxCleaningDuration, idxTotalDuration }) + 1;
        if (cols.size() < needSize)
            continue;

        CleaningDashboardRecord r;
        r.id = (idxId >= 0) ? cols[idxId].trimmed() : "";

        if (idxTowerTriggeringTime >= 0)
        {
            QString dtStr = cols[idxTowerTriggeringTime].trimmed();

            // Try common formats
            r.towerTriggeringTime = QDateTime::fromString(dtStr, Qt::ISODate);
            if (!r.towerTriggeringTime.isValid())
                r.towerTriggeringTime = QDateTime::fromString(dtStr, "yyyy-MM-dd HH:mm:ss");
            if (!r.towerTriggeringTime.isValid())
                r.towerTriggeringTime = QDateTime::fromString(dtStr, "dd/MM/yyyy HH:mm:ss");
        }

        if (idxTriggerToStart >= 0)
            r.triggerToStartCleaningDuration = cols[idxTriggerToStart].trimmed().toDouble();

        if (idxCleaningDuration >= 0)
            r.cleaningDuration = cols[idxCleaningDuration].trimmed().toDouble();

        if (idxTotalDuration >= 0)
            r.totalDuration = cols[idxTotalDuration].trimmed().toDouble();

        records.push_back(r);
    }

    return records;
}

QChartView* Moonica_qt::createDurationChart(
    const QString& title,
    const QVector<CleaningDashboardRecord>& records,
    std::function<double(const CleaningDashboardRecord&)> valueGetter)
{
    QLineSeries* series = new QLineSeries();
    series->setName(title);

    double maxY = 0.0;

    for (int i = 0; i < records.size(); ++i)
    {
        double y = valueGetter(records[i]);
        series->append(i + 1, y);

        if (y > maxY)
            maxY = y;
    }

    QChart* chart = new QChart();
    chart->addSeries(series);
    chart->setTitle(title);
    chart->legend()->hide();
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->setTheme(QChart::ChartThemeDark);

    QValueAxis* axisX = new QValueAxis();
    axisX->setTitleText("Record");
    axisX->setLabelFormat("%d");
    axisX->setRange(1, qMax(1, records.size()));

    QValueAxis* axisY = new QValueAxis();
    axisY->setTitleText("Duration (sec)");
    axisY->setLabelFormat("%.2f");
    axisY->setRange(0, maxY > 0.0 ? maxY * 1.15 : 10.0);

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);

    series->attachAxis(axisX);
    series->attachAxis(axisY);

    QChartView* chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setMinimumHeight(260);

    return chartView;
}