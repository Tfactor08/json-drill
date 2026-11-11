#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <raylib.h>

#define WIDTH 1120
#define HEIGHT 840
#define PLOT_MARGIN 50
#define POINT_SIZE 5
#define FONT_SIZE 30

#define BACKGROUND     (Color) { 0x22, 0x22, 0x22, 0xff }
#define GRAPH_COLOR1   (Color) { 0,  173, 181, 255}
#define GRAPH_COLOR2   (Color) { 255, 211, 105, 255}
#define INVALID_COLOR  (Color) { 255, 107, 107, 255}

#define MAX_SAMPLES 2000
#define MEASURE_COUNT 10
#define PIXELS_PER_MEASURE (WIDTH / MEASURE_COUNT)

#define PYTHON_MODULE_PATH "./preprocessor/preprocessor.py"
#define OUTPUT_FILE        "./total_results.txt"

#define STAT_COUNT 9

typedef struct {
    float t, rpm, pressure;
    bool isInvalid;
} Sample;

typedef struct {
    float t;
    struct {
        float rpm;
        float pressure;
    } values;
    float rpmX, rpmY;
    float pressureX, pressureY;
    bool isGap;
} Point;

typedef struct {
    float t;
    float x;
} InvalidPoint;

typedef struct {
    const char *title;
    float value;
    Rectangle bar;
    Color color;
} DatasetStat;

typedef enum {
    STATE_PLOT,
    STATE_RESULTS,
    __renderStateCount
} RenderState;

const char *statTitles[STAT_COUNT] = {
    "unknown_s", "working_s", "span_s", "stopped_s",
    "invalid_s", "gap_s", "low", "normal", "high"
};

const Color statColors[STAT_COUNT] = {
    { 34,  40,  49, 255},  // Dark slate
    { 57,  62,  70, 255},  // Charcoal
    { 0,  173, 181, 255},  // Teal
    { 238, 238, 238, 255}, // Off-white
    { 255, 211, 105, 255}, // Amber
    { 255, 107, 107, 255}, // Coral
    { 108, 117, 125, 255}, // Gray
    { 155, 246, 255, 255}, // Ice blue
    { 168, 224, 99, 255}   // Lime green
};

RenderState renderState = STATE_PLOT;

// TODO: Allocate memory for samples dynamically
Sample samples[MAX_SAMPLES];
Point points[MAX_SAMPLES];
InvalidPoint invalidPoints[MAX_SAMPLES];
DatasetStat stats[STAT_COUNT];

Font font;

// TODO: Maybe organize these metadata into a struct?
float lowerRpm, upperRpm;
float lowerPressure, upperPressure;
int timeSpan;
int sampleCount;
int pointCount;
int invalidPointCount;

// Convert from [lowerY..upperY] to [PLOT_MARGIN..HEIGHT-PLOT_MARGIN]
float ScreenY(float y, float lowerY, float upperY)
{
    // [lowerY..upperY]   - lowerY
    // [0..upperY-lowerY] / upperY-lowerY
    // [0..1] * HEIGHT
    if (lowerY == upperY)
        return 0 * (HEIGHT - 2*PLOT_MARGIN) + PLOT_MARGIN;
    else
        return (1 - ((y - lowerY) / (upperY-lowerY))) * (HEIGHT - 2*PLOT_MARGIN) + PLOT_MARGIN;
}

// Convert from [0..timeSpan] to [PLOT_MARGIN..WIDTH-PLOT_MARGIN]
float ScreenX(float x)
{
    // [0..timeSpan] / timeSpan
    // [0..1] * WIDTH
    return (x / timeSpan) * (WIDTH - 2*PLOT_MARGIN) + PLOT_MARGIN;
}

void RenderAxes(void)
{
    // Vertical Axis
    DrawLine(PLOT_MARGIN, HEIGHT-PLOT_MARGIN, PLOT_MARGIN, PLOT_MARGIN, WHITE);    
    // Horizontal Axis
    DrawLine(PLOT_MARGIN, HEIGHT-PLOT_MARGIN, WIDTH-PLOT_MARGIN, HEIGHT-PLOT_MARGIN, WHITE);    
    // Top arrow
    DrawTriangle(
        (Vector2) { PLOT_MARGIN, PLOT_MARGIN },
        (Vector2) { PLOT_MARGIN-6, PLOT_MARGIN+10 },
        (Vector2) { PLOT_MARGIN+6, PLOT_MARGIN+10  },
        WHITE
    );
    // Right arrow
    DrawTriangle(
        (Vector2) { WIDTH-PLOT_MARGIN, HEIGHT-PLOT_MARGIN },
        (Vector2) { WIDTH-PLOT_MARGIN-10, HEIGHT-PLOT_MARGIN-6 },
        (Vector2) { WIDTH-PLOT_MARGIN-10, HEIGHT-PLOT_MARGIN+6  },
        WHITE
    );
}

void SetPointsScreenCoords(void)
{
    for (int i = 0; i < pointCount; i++) {
        Point *point = &points[i];
        point->rpmX = ScreenX(point->t);
        point->pressureX = ScreenX(point->t);
        point->rpmY = ScreenY(point->values.rpm, lowerRpm, upperRpm);
        point->pressureY = ScreenY(point->values.pressure, lowerPressure, upperPressure);
    }
    for (int i = 0; i < invalidPointCount; i++) {
        InvalidPoint *point = &invalidPoints[i];
        point->x = ScreenX(point->t);
    }
}

void RenderPlot()
{
    SetPointsScreenCoords();
    Point firstPoint;
    for (int i = 0; i < pointCount; i++) {
        if (i == 0) {
            firstPoint = points[0];
            DrawCircle(firstPoint.rpmX, firstPoint.rpmY, POINT_SIZE, GRAPH_COLOR1);
            DrawCircle(firstPoint.pressureX, firstPoint.pressureY, POINT_SIZE, GRAPH_COLOR2);
            continue;
        }
        Point secondPoint = points[i];

        if (firstPoint.isGap) {
            DrawLineEx((Vector2) { firstPoint.rpmX, firstPoint.rpmY },
                       (Vector2) { secondPoint.rpmX, secondPoint.rpmY },
                       4, GRAY);
            DrawLineEx((Vector2) { firstPoint.pressureX, firstPoint.pressureY },
                       (Vector2) { secondPoint.pressureX, secondPoint.pressureY },
                       4, GRAY);
        } else {
            DrawLine(firstPoint.rpmX, firstPoint.rpmY,
                     secondPoint.rpmX, secondPoint.rpmY,
                     GRAPH_COLOR1);
            DrawLine(firstPoint.pressureX, firstPoint.pressureY,
                     secondPoint.pressureX, secondPoint.pressureY,
                     GRAPH_COLOR2);
        }

        DrawCircle(secondPoint.rpmX, secondPoint.rpmY, POINT_SIZE, GRAPH_COLOR1);
        DrawCircle(secondPoint.pressureX, secondPoint.pressureY, POINT_SIZE, GRAPH_COLOR2);

        firstPoint = secondPoint;
    }
}

void RenderInvalidPoints(void)
{
    for (int i = 0; i < invalidPointCount; i++) {
        InvalidPoint point = invalidPoints[i];
        DrawLine(point.x, PLOT_MARGIN, point.x, HEIGHT-PLOT_MARGIN, INVALID_COLOR);
    }
}

FILE *OpenFile(const char *input_file)
{
    FILE *f = fopen(input_file, "r");
    if (!f) {
        fprintf(stderr, "ERROR: could not open file %s\n", input_file);
        exit(1);
    }
    return f;
}

int CheckDatasetStatus(FILE *f)
{
    char buf[64];
    if (fgets(buf, sizeof(buf), f)) {
        if (strcmp(buf, "ok\n") == 0)
            return 0;
        do {
            printf("%s", buf);
        } while (fgets(buf, sizeof(buf), f));
        exit(1);
    } else {
        return -1;
    }
}

int LoadStats(FILE *f)
{
    for (int stati = 0; stati < STAT_COUNT; stati++) {
        DatasetStat *stat = &(stats[stati]);
        *stat = (DatasetStat) {
            .title = statTitles[stati],
            .color = statColors[stati]
        };
        if (fscanf(f, "%f", &(stat->value)) != 1)
            return -1;
    }
    return 0;
}

int LoadSamples(FILE *f)
{
    Sample sampleBuf = { .isInvalid = false };
    int samplei = 0;
    int c;

    if (fscanf(f, "%d", &sampleCount) != 1)
        return -1;
    if (fscanf(f, "%d", &timeSpan) != 1)
        return -1;
    if (fscanf(f, "%f%f", &lowerRpm, &upperRpm) != 2)
        return -1;
    if (fscanf(f, "%f%f", &lowerPressure, &upperPressure) != 2)
        return -1;

    while (true) {
        if (fscanf(f, "%f", &(sampleBuf.t)) == -1)
            break;
        if (fscanf(f, "%f", &(sampleBuf.rpm)) != 1) {  // If n is encountered
            sampleBuf.isInvalid = true;
            //while ((c = fgetc(f)) != EOF && c != '\n')
            //    continue;
        } else {
            if (fscanf(f, "%f", &(sampleBuf.pressure)) != 1)
                return -1;
        }
        samples[samplei++] = sampleBuf;
        sampleBuf.isInvalid = false;
    }

    fclose(f);

    return 0;
}

void CallPythonModule(const char *input_file)
{
    char python_call_cmd[128];
    snprintf(python_call_cmd, 128,
             "python %s %s raw %s", PYTHON_MODULE_PATH, input_file, OUTPUT_FILE);
    system(python_call_cmd); // TODO: Check for errors
}

void CompressSamples()
{
    float batchSize = (float) timeSpan / MEASURE_COUNT;
    float accumulatedInterval = 0.0f;
    float accumulatedRpm = 0.0f, accumulatedPressure = 0.0f;

    for (int samplei = 0; samplei < sampleCount; samplei++) {
        Sample current = samples[samplei];
        if (current.isInvalid == true) {
            invalidPoints[invalidPointCount++] = (InvalidPoint) {
                .t = current.t
            };
            continue;
        }
        if (samplei == 0) {
            points[pointCount++] = (Point) {
                .t = samples[0].t,
                .values.rpm = samples[0].rpm,
                .values.pressure = samples[0].pressure,
                .isGap = false
            };
            continue;
        } else if (samplei == sampleCount - 1) {
            points[pointCount++] = (Point) {
                .t = samples[samplei].t,
                .values.rpm = accumulatedRpm / accumulatedInterval,
                .values.pressure = accumulatedPressure / accumulatedInterval,
                .isGap = false
            };
            break;
        }
        Sample next = samples[samplei + 1];
        float interval = next.t - current.t;
        if (interval > 1.5f) {
            points[pointCount++] = (Point) {
                .t = current.t,
                .values.rpm = accumulatedRpm / accumulatedInterval,
                .values.pressure = accumulatedPressure / accumulatedInterval,
                .isGap = true
            };
            points[pointCount++] = (Point) {
                .t = next.t,
                .values.rpm = next.rpm,
                .values.pressure = next.pressure,
                .isGap = false
            };
            accumulatedInterval = next.rpm - batchSize*((int) (next.rpm / batchSize));
            accumulatedRpm = accumulatedPressure = 0.0f;
            continue;
        }
        if (batchSize - accumulatedInterval <= 0) {
            points[pointCount++] = (Point) {
                .t = current.t,
                .values.rpm = accumulatedRpm / accumulatedInterval,
                .values.pressure = accumulatedPressure / accumulatedInterval,
                .isGap = false
            };
            accumulatedInterval = accumulatedRpm = accumulatedPressure = 0.0f;
        }
        accumulatedInterval += interval;
        accumulatedRpm += current.rpm;
        accumulatedPressure += current.pressure;
    }
}

void ShowPointDetails(Point *point)
{
    char pointInfoBuf[128];
    snprintf(pointInfoBuf, 128, "t: %.2f\nrpm: %.2f\npressure: %.2f\n", point->t, point->values.rpm, point->values.pressure);
    // TODO
    //int textPosX = point->x - 20;
    //int textPosY = point->y / HEIGHT > 0.6f ? point->y - 70 : point->y + 70;
    int textPosX = PLOT_MARGIN + 20;
    int textPosY = PLOT_MARGIN + 20;
    DrawTextEx(font, pointInfoBuf, (Vector2) { textPosX, textPosY }, FONT_SIZE, 2, WHITE);
}

void ProcessPoints(void)
{
    for (int pointi = 0; pointi < pointCount; pointi++) {
        Point point = points[pointi];
        Vector2 centerRpm = { .x = point.rpmX, .y = point.rpmY };
        bool collisionRpm = CheckCollisionPointCircle(GetMousePosition(), centerRpm, POINT_SIZE);
        Vector2 centerPressure = { .x = point.pressureX, .y = point.pressureY };
        bool collisionPressure = CheckCollisionPointCircle(GetMousePosition(), centerPressure, POINT_SIZE);
        if (!(collisionRpm || collisionPressure)) continue;
        ShowPointDetails(&point);
    }
}

void CreateResultBars(void)
{
    const int maxHeight = 100;
    const int width = 40;
    //const int marginX = (WIDTH - width*STAT_COUNT) / (STAT_COUNT-2);
    const int marginX = width*2;
    const int marginY = HEIGHT/2 - 100;
    float referenceTime = timeSpan;
    for (int i = 0; i < STAT_COUNT; i++) {
        DatasetStat *stat = &stats[i];
        int height = maxHeight * (stat->value / referenceTime);
        if (height == 0) height = 4;
        float G = width*1.5;
        float M = (WIDTH - (STAT_COUNT-1)*G - STAT_COUNT*width) / 2.0f;
        float x = M + i * (width + G);
        stat->bar = (Rectangle) {
            .x = x,
            .y = HEIGHT-marginY-height,
            .width = width,
            .height = height
        };
    }
}

void RenderResultBars(void)
{
    for (int i = 0; i < STAT_COUNT; i++) {
        DatasetStat stat = stats[i];
        DrawRectangleRec(stat.bar, stat.color);
    }
}

void ShowStatDetails(DatasetStat *stat)
{
    char statInfoBuf[128];
    Rectangle bar = stat->bar;
    snprintf(statInfoBuf, 128, "%s: %.2f", stat->title, stat->value);
    int textPosX = bar.x - 50;
    int textPosY = HEIGHT - (bar.height + bar.y) + 50;
    DrawTextEx(font, statInfoBuf, (Vector2) { textPosX, textPosY }, FONT_SIZE, 2, WHITE);
}

void ProcessResultBars(void)
{
    for (int i = 0; i < STAT_COUNT; i++) {
        DatasetStat stat = stats[i];
        Rectangle bar = stat.bar;
        bool collision = CheckCollisionPointRec(GetMousePosition(), bar);
        if (!collision) continue;
        ShowStatDetails(&stat);
    }
}

void HandleRenderState(void)
{
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT)) {
        renderState = (renderState + 1) % __renderStateCount;
    }
    if (renderState == STATE_PLOT) {
        RenderAxes();
        RenderPlot();
        RenderInvalidPoints();
        ProcessPoints();
    } else if (renderState == STATE_RESULTS) {
        RenderResultBars();
        ProcessResultBars();
    } else {
        assert(0 && "Unreachable");
    }
}

int main(int argc, char *argv[])
{
    //FILE *f = OpenFile("total_results.tmp");
    //if (CheckDatasetStatus(f) != 0) {
    //    fprintf(stderr, "ERROR: bad file format: %s", OUTPUT_FILE);
    //    exit(1);
    //}
    //if (LoadStats(f) != 0) {
    //    fprintf(stderr, "ERROR: bad file format: %s", OUTPUT_FILE);
    //    exit(1);
    //}
    //if (LoadSamples(f) != 0) {
    //    fprintf(stderr, "ERROR: bad file format: %s", OUTPUT_FILE);
    //    exit(1);
    //}
    //for (int i = 0; i < sampleCount; i++) {
    //    printf("%f, invalid: %d, rpm: %f, pressure: %f\n", samples[i].t, samples[i].isInvalid, samples[i].rpm, samples[i].pressure);
    //}
    //CompressSamples();
    //SetPointsScreenCoords();

    //return 0;

    if (argc < 2) {
        fprintf(stderr, "USAGE: %s dataset\n", argv[0]);
        exit(1);
    }
    const char *dataset = argv[1];

    CallPythonModule(dataset);

    FILE *f = OpenFile(OUTPUT_FILE);
    if (CheckDatasetStatus(f) != 0) {
        fprintf(stderr, "ERROR: bad file format: %s", OUTPUT_FILE);
        exit(1);
    }
    if (LoadStats(f) != 0) {
        fprintf(stderr, "ERROR: bad file format: %s", OUTPUT_FILE);
        exit(1);
    }
    if (LoadSamples(f) != 0) {
        fprintf(stderr, "ERROR: bad file format: %s", OUTPUT_FILE);
        exit(1);
    }

    CompressSamples();
    CreateResultBars();

    InitWindow(1120, 840, "Plotter");
    SetTargetFPS(20);

    font = LoadFont("/home/sserped/.local/share/fonts/ttf/JetBrainsMono-Regular.ttf");

    while (!WindowShouldClose()) {
        BeginDrawing();
            HandleRenderState();
            ClearBackground(BACKGROUND);
        EndDrawing();
    }

    return 0;
}
