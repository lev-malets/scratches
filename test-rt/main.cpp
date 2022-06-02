#include <QtCharts>
#include <QtCore>
#include <QtGui>
#include <QtWidgets>
#include <rtaudio/RtAudio.h>

using namespace std;

constexpr size_t UPDATE_FREQ = 10;
constexpr size_t UPDATE_TIMEOUT = 1000 / UPDATE_FREQ;
constexpr size_t TIMELINE_LENGTH = 4;
constexpr size_t CHART_PRECISION = 32;

class Chunk {
public:
  unique_ptr<Chunk> previous;
  uint8_t *samples_agg;

  Chunk(size_t chunkSize) : samples_agg(new uint8_t[chunkSize]) {}
  ~Chunk() { delete[] samples_agg; }
};

class Samples {
public:
  const size_t chunkSize;
  const size_t pointsCount;
  const double timeStep;
  const size_t aggSize;

  unique_ptr<Chunk> lastChunk;
  size_t lastChunkSize{};

  int_fast32_t lastAggMax{};
  size_t lastAggSize{};

  static unique_ptr<Samples> make(size_t sampleRate) {
    size_t aggSize = sampleRate / CHART_PRECISION;
    double timeStep = 1 / ((double)sampleRate / aggSize);
    size_t pointsCount = TIMELINE_LENGTH * (double)sampleRate / aggSize;
    size_t chunkSize = pointsCount;

    return unique_ptr<Samples>(
        new Samples(chunkSize, pointsCount, timeStep, aggSize));
  }

  void addBuffer(int16_t *buffer, unsigned int nFrames) {
    for (unsigned int i = 0; i < nFrames; i++) {
      int_fast32_t v = abs(buffer[i]);
      if (v > lastAggMax) {
        lastAggMax = v;
      }
      lastAggSize += 1;
      if (lastAggSize == aggSize) {
        uint8_t level = lastAggMax * 100 / 0x7FFF;
        Chunk *chunk;
        if (lastChunkSize == chunkSize) {
          auto oldHead = lastChunk.release();
          chunk = new Chunk(chunkSize);
          chunk->previous.reset(oldHead);
          lastChunk.reset(chunk);
          lastChunkSize = 0;
        } else {
          chunk = lastChunk.get();
        }

        chunk->samples_agg[lastChunkSize] = level;
        lastChunkSize += 1;
        lastAggMax = 0;
        lastAggSize = 0;
      }
    }
  }

  Chunk *fillSeries(QLineSeries *series) {
    series->clear();
    auto idx = lastChunkSize;
    auto chunk = lastChunk.get();
    for (size_t i = 0; i < pointsCount; i++) {
      if (idx == 0) {
        auto prev = chunk->previous.get();
        if (prev == nullptr) {
          if (i < pointsCount - 1) {
            series->append(-(i * timeStep), 0);
          }
          auto last_i = pointsCount - 1;
          series->append(-(last_i * timeStep), 0);
          break;
        }
        chunk = prev;
        idx = chunkSize;
      }
      idx -= 1;
      series->append(-(i * timeStep), chunk->samples_agg[idx]);
    }

    return chunk;
  }

private:
  Samples(size_t chunkSize, size_t pointsCount, double timeStep, size_t aggSize)
      : chunkSize(chunkSize), pointsCount(pointsCount), timeStep(timeStep),
        aggSize(aggSize), lastChunk(new Chunk(chunkSize)) {}
};

class Chart : public QChart {
public:
  Samples *samples;
  QLineSeries *series;

  Chart(Samples *queue) : samples(queue), series(new QLineSeries()) {
    queue->fillSeries(series);
    addSeries(series);
    createDefaultAxes();
    this->axes(Qt::Vertical, series)[0]->setRange(0, 100);
    this->axes(Qt::Horizontal, series)[0]->setRange(-(int)TIMELINE_LENGTH, 0);

    legend()->hide();

    auto timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &Chart::updateLine);
    timer->start(UPDATE_TIMEOUT);
  }
public slots:
  void updateLine() {
    auto last_used_chunk = samples->fillSeries(this->series);
    last_used_chunk->previous.reset();
  }
};

int record_cb(void *, void *inputBuffer, unsigned int nFrames, double,
              RtAudioStreamStatus status, void *userData) {
  auto samples = (Samples *)userData;
  auto buffer = (int16_t *)inputBuffer;

  samples->addBuffer(buffer, nFrames);

  return 0;
}

int main(int argc, char *argv[]) {
  RtAudio audio;
  auto device = audio.getDefaultInputDevice();
  auto info = audio.getDeviceInfo(device);
  auto formats = info.nativeFormats;
  if ((formats & RTAUDIO_SINT16) == 0) {
    cerr << "format RTAUDIO_SINT16 is not supported" << endl;
    exit(1);
  }
  auto minRate = *min_element(info.sampleRates.begin(), info.sampleRates.end());

  RtAudio::StreamParameters parameters = {
      .deviceId = device,
      .nChannels = 1,
  };

  auto samples = Samples::make(minRate);

  QApplication app(argc, argv);
  QChartView view(new Chart(samples.get()));
  view.setRenderHint(QPainter::Antialiasing);
  view.setMinimumSize(QSize(800, 400));
  view.show();

  unsigned int bufferFrames = 100;

  try {
    audio.openStream(nullptr, &parameters, RTAUDIO_SINT16, minRate,
                     &bufferFrames, &record_cb, samples.get());
    audio.startStream();
  } catch (RtAudioError &e) {
    e.printMessage();
    exit(1);
  }

  int ret = app.exec();

  try {
    audio.stopStream();
  } catch (RtAudioError &e) {
    e.printMessage();
  }
  if (audio.isStreamOpen())
    audio.closeStream();

  return ret;
}
