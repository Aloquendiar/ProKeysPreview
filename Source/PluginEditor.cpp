#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <vector>
#include <limits>
#include <cmath>
#include <algorithm>

//==============================================================================
ProKeysPreviewAudioProcessorEditor::ProKeysPreviewAudioProcessorEditor(ProKeysPreviewAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setResizeLimits(200, 150, 4096, 2160);
    setResizable(true, false); // allow host resizing but hide corner resizer
    setSize(540, 700); // initial size; window can be resized in host
    startTimerHz(audioProcessor.refreshRateHz.load()); // refresh rate

    // Setup difficulty label
    difficultyLabel.setText("Difficulty:", juce::dontSendNotification);
    difficultyLabel.setJustificationType(juce::Justification::centredRight);
    difficultyLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    difficultyLabel.setFont(juce::Font(13.0f));
    addAndMakeVisible(difficultyLabel);

    // Setup difficulty combo box
    difficultyComboBox.addItem("This track", 1);
    difficultyComboBox.addItem("Expert", 2);
    difficultyComboBox.addItem("Hard", 3);
    difficultyComboBox.addItem("Medium", 4);
    difficultyComboBox.addItem("Easy", 5);
    difficultyComboBox.setSelectedId(audioProcessor.difficultySelection.load() + 1, juce::dontSendNotification);
    difficultyComboBox.onChange = [this]()
    {
        int selectedId = difficultyComboBox.getSelectedId();
        if (selectedId > 0)
        {
            int difficulty = selectedId - 1;
            audioProcessor.difficultySelection.store(difficulty);
        }
    };
    addAndMakeVisible(difficultyComboBox);

    // Setup time offset label
    timeOffsetLabel.setText("Time Offset (ms):", juce::dontSendNotification);
    timeOffsetLabel.setJustificationType(juce::Justification::centredRight);
    timeOffsetLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    timeOffsetLabel.setFont(juce::Font(13.0f));
    addAndMakeVisible(timeOffsetLabel);

    // Setup time offset editor
    timeOffsetEditor.setFont(juce::Font(12.0f));
    timeOffsetEditor.setIndents(4, 3);
    timeOffsetEditor.setText(juce::String(audioProcessor.timeOffsetSeconds.load() * 1000.0), false);
    timeOffsetEditor.setInputRestrictions(0, "0123456789.-");
    timeOffsetEditor.setJustification(juce::Justification::centred);
    timeOffsetEditor.onTextChange = [this]()
    {
        double offsetMs = timeOffsetEditor.getText().getDoubleValue();
        double offsetSeconds = offsetMs / 1000.0;
        audioProcessor.timeOffsetSeconds.store(offsetSeconds);
        audioProcessor.saveTimeOffset(offsetSeconds);
    };
    addAndMakeVisible(timeOffsetEditor);

    // Setup refresh rate label
    refreshRateLabel.setText("Refresh Rate (Hz):", juce::dontSendNotification);
    refreshRateLabel.setJustificationType(juce::Justification::centredRight);
    refreshRateLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    refreshRateLabel.setFont(juce::Font(13.0f));
    addAndMakeVisible(refreshRateLabel);

    // Setup refresh rate editor
    refreshRateEditor.setFont(juce::Font(12.0f));
    refreshRateEditor.setIndents(4, 3);
    refreshRateEditor.setText(juce::String(audioProcessor.refreshRateHz.load()), false);
    refreshRateEditor.setInputRestrictions(0, "0123456789");
    refreshRateEditor.setJustification(juce::Justification::centred);
    refreshRateEditor.onTextChange = [this]()
    {
        int rateHz = refreshRateEditor.getText().getIntValue();
        if (rateHz > 0 && rateHz <= 500)  // Clamp to reasonable range
        {
            audioProcessor.refreshRateHz.store(rateHz);
            audioProcessor.saveRefreshRate(rateHz);
            startTimerHz(rateHz);  // Update timer immediately
        }
    };
    addAndMakeVisible(refreshRateEditor);

    // Setup track speed label
    trackSpeedLabel.setText("Speed:", juce::dontSendNotification);
    trackSpeedLabel.setJustificationType(juce::Justification::centredRight);
    trackSpeedLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    trackSpeedLabel.setFont(juce::Font(13.0f));
    addAndMakeVisible(trackSpeedLabel);

    // Setup track speed combo box
    constexpr int speedOptions[] = { 50, 75, 100, 125, 150, 175, 200 };
    for (int i = 0; i < 7; ++i)
        trackSpeedComboBox.addItem(juce::String(speedOptions[i]) + "%", i + 1);
    {
        const int currentSpeed = audioProcessor.trackSpeedPercent.load();
        int initialId = 3; // default 100%
        for (int i = 0; i < 7; ++i)
            if (speedOptions[i] == currentSpeed) { initialId = i + 1; break; }
        trackSpeedComboBox.setSelectedId(initialId, juce::dontSendNotification);
    }
    trackSpeedComboBox.onChange = [this]()
    {
        constexpr int speeds[] = { 50, 75, 100, 125, 150, 175, 200 };
        const int selectedId = trackSpeedComboBox.getSelectedId();
        if (selectedId > 0 && selectedId <= 7)
        {
            const int speed = speeds[selectedId - 1];
            audioProcessor.trackSpeedPercent.store(speed);
            audioProcessor.saveTrackSpeed(speed);
        }
    };
    addAndMakeVisible(trackSpeedComboBox);

    auto loadBinaryImage = [](const void* data, int dataSize) -> juce::Image
    {
        return juce::ImageCache::getFromMemory(data, dataSize);
    };

    whiteGemImg = loadBinaryImage(BinaryData::pk_white_png, BinaryData::pk_white_pngSize);
    blackGemImg = loadBinaryImage(BinaryData::pk_black_png, BinaryData::pk_black_pngSize);
    whiteGlissGemImg = loadBinaryImage(BinaryData::pk_white_gliss_png, BinaryData::pk_white_gliss_pngSize);
    blackErrorGemImg = loadBinaryImage(BinaryData::pk_black_error_png, BinaryData::pk_black_error_pngSize);
    whiteErrorGemImg = loadBinaryImage(BinaryData::pk_white_error_png, BinaryData::pk_white_error_pngSize);
    whiteOdGemImg = loadBinaryImage(BinaryData::pk_white_od_png, BinaryData::pk_white_od_pngSize);
    blackOdGemImg = loadBinaryImage(BinaryData::pk_black_od_png, BinaryData::pk_black_od_pngSize);

    const char* const smashData[] = {
        BinaryData::smash_1_png, BinaryData::smash_2_png, BinaryData::smash_3_png, BinaryData::smash_4_png, BinaryData::smash_5_png,
        BinaryData::smash_6_png, BinaryData::smash_7_png, BinaryData::smash_8_png, BinaryData::smash_9_png, BinaryData::smash_10_png
    };
    const int smashSizes[] = {
        BinaryData::smash_1_pngSize, BinaryData::smash_2_pngSize, BinaryData::smash_3_pngSize, BinaryData::smash_4_pngSize, BinaryData::smash_5_pngSize,
        BinaryData::smash_6_pngSize, BinaryData::smash_7_pngSize, BinaryData::smash_8_pngSize, BinaryData::smash_9_pngSize, BinaryData::smash_10_pngSize
    };

    for (int i = 0; i < 10; ++i)
        smashImgs[i] = loadBinaryImage(smashData[i], smashSizes[i]);

    smashFlareImg = loadBinaryImage(BinaryData::smash_flare_style_png, BinaryData::smash_flare_style_pngSize);
}

ProKeysPreviewAudioProcessorEditor::~ProKeysPreviewAudioProcessorEditor()
{
}

//==============================================================================
void ProKeysPreviewAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Fill the background solid black
    g.fillAll(juce::Colours::black);

    const auto bounds = getLocalBounds().toFloat().reduced(10.0f);

    // Display error message if present
    if (audioProcessor.errorMessage.isNotEmpty())
    {
        g.setColour(juce::Colours::red);
        g.drawText(audioProcessor.errorMessage, bounds, juce::Justification::centred);
        return;
    }

    // Define the time window scaled by track speed (higher speed = shorter window)
    // Each difficulty has a base speed relative to Expert: Hard 75%, Medium 50%, Easy 25%
    constexpr double difficultySpeedFactors[] = { 1.0, 1.0, 0.75, 0.50, 0.25 };
    const int difficulty = juce::jlimit(0, 4, audioProcessor.difficultySelection.load());
    const double effectiveSpeed = static_cast<double>(audioProcessor.trackSpeedPercent.load()) * difficultySpeedFactors[difficulty];
    const double speedScale = 100.0 / std::max(effectiveSpeed, 1.0);
    const double totalVisibleTime = 1.25 * speedScale;
    const double timeBeforePlayhead = totalVisibleTime * (1.0 / 7.0);
    const double timeAfterPlayhead  = totalVisibleTime * (6.0 / 7.0);

    // Read a consistent playback snapshot via seqlock so that position,
    // timestamp, rate and playing flag always come from the same processBlock call.
    double snapPosition, snapUpdateTime, rate;
    bool playing;
    {
        unsigned s1, s2;
        do {
            s1           = audioProcessor.playbackSeq.load(std::memory_order_acquire);
            snapPosition   = audioProcessor.currentPlaybackPosition.load(std::memory_order_relaxed);
            playing        = audioProcessor.isPlaying.load(std::memory_order_relaxed);
            snapUpdateTime = audioProcessor.positionUpdateTime.load(std::memory_order_relaxed);
            rate           = audioProcessor.playbackRate.load(std::memory_order_relaxed);
            s2           = audioProcessor.playbackSeq.load(std::memory_order_acquire);
        } while (s1 != s2 || (s1 & 1));
    }

    // Only apply time offset during playback, not when scrubbing.
    // Scale by playback rate so the offset stays perceptually consistent
    // (e.g. at 0.5x speed the project-time offset is halved).
    const double rawOffset = audioProcessor.timeOffsetSeconds.load();
    const double timeOffset = playing ? rawOffset * rate : 0.0;

    // Interpolate the playback position forward from the last audio-thread
    // update so that note movement is smooth between processBlock calls.
    const double paintNow = juce::Time::getMillisecondCounterHiRes() * 0.001;
    double rawInterpolated = snapPosition;
    if (playing)
    {
        const double elapsed = paintNow - snapUpdateTime;
        if (elapsed > 0.0 && elapsed < 0.1)
            rawInterpolated += elapsed * rate;
    }

    // Smooth the displayed position to eliminate micro-jitter caused by
    // wall-clock / audio-clock drift at audio-buffer boundaries.  A short
    // exponential-decay filter blends the predicted continuation of the
    // last frame with the freshly interpolated target.
    const double dt = m_lastPaintTime > 0.0 ? (paintNow - m_lastPaintTime) : 0.0;
    m_lastPaintTime = paintNow;

    if (!playing || !m_wasPlaying || std::abs(rawInterpolated - m_smoothedTime) > 0.5)
    {
        m_smoothedTime = rawInterpolated;
    }
    else
    {
        const double predicted = m_smoothedTime + dt * rate;
        const double error     = rawInterpolated - predicted;
        const double alpha     = 1.0 - std::exp(-dt / 0.008); // 8 ms time constant
        m_smoothedTime = predicted + error * alpha;
    }
    m_wasPlaying = playing;

    const double currentTime = m_smoothedTime - timeOffset;
    const double windowStartTime = currentTime - timeBeforePlayhead;
    const double windowEndTime = currentTime + timeAfterPlayhead;

    // Determine active display range from range shift markers
    int displayPitchMin = 48; // default C2-E3
    int displayPitchMax = 64;
    const RangeShift* active = nullptr;
    if (!audioProcessor.rangeShifts.empty())
    {
        for (const auto& rs : audioProcessor.rangeShifts)
        {
            if (rs.time <= currentTime)
                active = &rs;
            else
                break;
        }
        if (active != nullptr)
        {
            displayPitchMin = active->pitchMin;
            displayPitchMax = active->pitchMax;
        }
        else
        {
            // Before any range shift - use the first upcoming one
            displayPitchMin = audioProcessor.rangeShifts.front().pitchMin;
            displayPitchMax = audioProcessor.rangeShifts.front().pitchMax;
        }
    }
    const int numLanes = displayPitchMax - displayPitchMin + 1;

    // Find next upcoming range shift for preview indicators
    const RangeShift* nextShift = nullptr;
    for (const auto& rs : audioProcessor.rangeShifts)
    {
        if (rs.time > currentTime)
        {
            nextShift = &rs;
            break;
        }
    }

    // Piano-style layout: white keys form the base lanes, black keys overlay the
    // boundary between adjacent white keys (matching the keyboard strikeline)
    auto isPitchBlackKey = [](int pitch) -> bool {
        const int pc = pitch % 12;
        return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
    };

    int numWhiteKeys = 0;
    for (int i = 0; i < numLanes; ++i)
        if (!isPitchBlackKey(displayPitchMin + i)) ++numWhiteKeys;

    // Dynamic highway layout: proportional padding above, vertically centred when tall
    const float controlsBottom = 50.0f;
    constexpr float paddingRatio = 0.14f; // padding above highway as fraction of highway height
    constexpr float aspectRatio = 10.0f / 7.0f;

    const float availableWidth = bounds.getWidth();
    const float totalAvailableH = static_cast<float>(getHeight()) - controlsBottom;

    // Fit highway to width first, then shrink if it doesn't fit vertically
    float highwayWidth = availableWidth;
    float highwayHeight = highwayWidth / aspectRatio;
    float topPadding = highwayHeight * paddingRatio;
    float totalNeededH = highwayHeight + topPadding;

    if (totalNeededH > totalAvailableH)
    {
        highwayHeight = totalAvailableH / (1.0f + paddingRatio);
        highwayWidth = highwayHeight * aspectRatio;
        topPadding = highwayHeight * paddingRatio;
        totalNeededH = totalAvailableH;
    }

    // Centre vertically when window is taller than needed
    float contentY = controlsBottom;
    if (totalNeededH < totalAvailableH)
        contentY += (totalAvailableH - totalNeededH) * 0.5f;

    const float highwayTop = contentY + topPadding;

    const float whiteKeyLaneWidth = highwayWidth / static_cast<float>(numWhiteKeys > 0 ? numWhiteKeys : 1);
    const float blackKeyLaneWidth = whiteKeyLaneWidth * 0.6f;
    const float highwayLeft = bounds.getCentreX() - highwayWidth * 0.5f;

    // Detect range changes during playback and start a horizontal swipe transition
    auto countWhiteKeysBetween = [](int fromPitch, int toPitch) -> int {
        auto isBlk = [](int p) {
            const int pc = p % 12;
            return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        };
        int count = 0;
        if (fromPitch < toPitch) {
            for (int p = fromPitch; p < toPitch; ++p)
                if (!isBlk(p)) ++count;
            return count;
        }
        for (int p = toPitch; p < fromPitch; ++p)
            if (!isBlk(p)) ++count;
        return -count;
    };

    const juce::uint32 nowMs = juce::Time::getMillisecondCounter();
    if (playing && m_prevDisplayPitchMin != -1 &&
        (displayPitchMin != m_prevDisplayPitchMin || displayPitchMax != m_prevDisplayPitchMax))
    {
        const int wkDelta = countWhiteKeysBetween(m_prevDisplayPitchMin, displayPitchMin);
        m_transitionInitialOffset = static_cast<float>(wkDelta) * whiteKeyLaneWidth;
        m_transitionStartMs = nowMs;
    }
    if (!playing)
        m_transitionInitialOffset = 0.0f;
    m_prevDisplayPitchMin = displayPitchMin;
    m_prevDisplayPitchMax = displayPitchMax;

    // Highway width is already determined by dynamic ratio limits


    // Precompute X position for every pitch:
    // - white keys are laid out sequentially left to right
    // - black keys are centred on the right edge of their left adjacent white key (always i-1)
    float pitchLaneX[25];
    {
        int whiteCount = 0;
        for (int i = 0; i < numLanes; ++i)
        {
            if (!isPitchBlackKey(displayPitchMin + i))
            {
                pitchLaneX[i] = highwayLeft + whiteCount * whiteKeyLaneWidth;
                ++whiteCount;
            }
            else
            {
                pitchLaneX[i] = pitchLaneX[i - 1] + whiteKeyLaneWidth - blackKeyLaneWidth * 0.5f;
            }
        }
    }

    // Compute current swipe offset and apply to all lane X positions
    constexpr float transitionDurationMs = 100.0f;
    const float transitionElapsed = static_cast<float>(
        std::min<juce::uint32>(nowMs - m_transitionStartMs, 10000u));
    const float currentOffset = m_transitionInitialOffset
        * (1.0f - juce::jlimit(0.0f, 1.0f, transitionElapsed / transitionDurationMs));
    if (currentOffset != 0.0f)
        for (int i = 0; i < numLanes; ++i)
            pitchLaneX[i] += currentOffset;

    // Compute X positions for all Pro Keys pitches (48-72) for out-of-range note rendering
    float allPitchX[25]; // index = pitch - 48
    {
        int whitesBefore = 0;
        for (int p = 48; p < displayPitchMin; ++p)
            if (!isPitchBlackKey(p)) ++whitesBefore;
        const float baseX = pitchLaneX[0] - whitesBefore * whiteKeyLaneWidth;
        int globalWhite = 0;
        for (int p = 48; p <= 72; ++p)
        {
            const int idx = p - 48;
            if (!isPitchBlackKey(p))
            {
                allPitchX[idx] = baseX + globalWhite * whiteKeyLaneWidth;
                ++globalWhite;
            }
            else
            {
                allPitchX[idx] = allPitchX[idx - 1] + whiteKeyLaneWidth - blackKeyLaneWidth * 0.5f;
            }
        }
    }

    // Partial black key shown at each edge if the adjacent pitch outside the range is a black key
    const int leftCandidatePitch = displayPitchMin - 1;
    const int rightCandidatePitch = displayPitchMax + 1;
    const int leftEdgePitch  = (!isPitchBlackKey(displayPitchMin)
                                && leftCandidatePitch >= 48
                                && isPitchBlackKey(leftCandidatePitch)) ? leftCandidatePitch : -1;
    const int rightEdgePitch = (!isPitchBlackKey(displayPitchMax)
                                && rightCandidatePitch <= 72
                                && isPitchBlackKey(rightCandidatePitch)) ? rightCandidatePitch : -1;
    const float leftEdgeX    = (leftEdgePitch  >= 0) ? pitchLaneX[0]            - blackKeyLaneWidth * 0.5f : 0.0f;
    const float rightEdgeX   = (rightEdgePitch >= 0) ? pitchLaneX[numLanes - 1] + whiteKeyLaneWidth - blackKeyLaneWidth * 0.5f : 0.0f;

    bool pitchActive[128] = { false };
    bool pitchSustaining[128] = { false };
    std::vector<int> pitchSmashFrames[128];

    const double safeRate = std::max(rate, 0.01);

    for (const auto& note : audioProcessor.parsedNotes)
    {
        if (note.pitch < 0 || note.pitch >= 128) continue;

        // Notes are sorted by startTime; skip if this note starts well after currentTime
        if (note.startTime > currentTime + 1.0)
            break;

        const double safeRateActive = std::max(rate, 0.01);
        double overDuration = std::max(0.250 * safeRateActive, note.endTime - note.startTime);
        if (currentTime >= note.startTime && currentTime <= note.startTime + overDuration)
        {
            pitchActive[note.pitch] = true;
        }

        if (playing && currentTime >= note.startTime && currentTime < note.startTime + 0.40 * safeRate)
        {
            double timeSinceStart = (currentTime - note.startTime) / safeRate;
            int frame = static_cast<int>(timeSinceStart / 0.040);
            if (frame >= 0 && frame < 10)
            {
                // Avoid duplicating exact same frame for same start times
                if (std::find(pitchSmashFrames[note.pitch].begin(), pitchSmashFrames[note.pitch].end(), frame) == pitchSmashFrames[note.pitch].end())
                {
                    pitchSmashFrames[note.pitch].push_back(frame);
                }
            }
        }

        if (currentTime >= note.startTime && currentTime <= note.endTime)
        {
            double sustainThreshold = 0.299 * 0.5; // fallback: 0.299 beats at 120 BPM (0.5 sec/beat)
            if (audioProcessor.TimeMap2_timeToBeats && audioProcessor.TimeMap2_beatsToTime)
            {
                double noteBeat = 0.0;
                int cdenom = 4;
                audioProcessor.TimeMap2_timeToBeats(nullptr, note.startTime, nullptr, nullptr, &noteBeat, &cdenom);
                const double scaledBeats = 0.299 * (cdenom / 4.0);
                const double t0 = audioProcessor.TimeMap2_beatsToTime(nullptr, noteBeat, nullptr);
                const double t1 = audioProcessor.TimeMap2_beatsToTime(nullptr, noteBeat + scaledBeats, nullptr);
                sustainThreshold = t1 - t0;
            }

            if ((note.endTime - note.startTime) >= sustainThreshold - 1e-4)
            {
                pitchSustaining[note.pitch] = true;
            }
        }
    }

    // 3D Perspective Math setup
    const float vpX = highwayLeft + highwayWidth * 0.5f;
    auto getScale = [&](float y) -> float
    {
        float ratio = juce::jlimit(0.0f, 1.0f, (y - highwayTop) / highwayHeight);
        float z = (10.0f / 3.0f) - ratio * ((10.0f / 3.0f) - 1.0f); // zFar=3.333 at top, zNear=1.0 at bottom
        return 1.0f / z;
    };
    auto projectPoint = [&](float x, float y) -> juce::Point<float>
    {
        float scale = getScale(y);
        float px = vpX + (x - vpX) * scale;
        float py = highwayTop + (y - highwayTop) * scale;
        return { px, py };
    };
    auto fillWarpedRectPath = [&](float x, float y, float w, float h)
    {
        static thread_local juce::Path p;
        p.clear();
        p.startNewSubPath(projectPoint(x, y));
        p.lineTo(projectPoint(x + w, y));
        p.lineTo(projectPoint(x + w, y + h));
        p.lineTo(projectPoint(x, y + h));
        p.closeSubPath();
        g.fillPath(p);
    };

    // Draw highway background
    g.setColour(juce::Colour(0xff181818));
    fillWarpedRectPath(highwayLeft, highwayTop, highwayWidth, highwayHeight);

    // Draw highway rails (thick lines on either side of the highway)
    {
        constexpr float railWidth = 12.0f;
        constexpr float soloEdgeWidth = 5.0f;
        const float leftRailX  = highwayLeft - railWidth;
        const float rightRailX = highwayLeft + highwayWidth;

        // Always draw the full grey rail
        g.setColour(juce::Colour(0xff555555));
        fillWarpedRectPath(leftRailX,  highwayTop, railWidth, highwayHeight);
        fillWarpedRectPath(rightRailX, highwayTop, railWidth, highwayHeight);

        // Overlay cyan edge strips during solo sections
        if (!audioProcessor.soloMarkers.empty())
        {
            auto isInSolo = [&](double time) -> bool
            {
                for (const auto& sm : audioProcessor.soloMarkers)
                {
                    if (time < sm.startTime)
                        break;
                    if (time >= sm.startTime && time < sm.endTime)
                        return true;
                }
                return false;
            };

            std::vector<double> boundaries;
            boundaries.push_back(windowStartTime);
            for (const auto& sm : audioProcessor.soloMarkers)
            {
                if (sm.startTime > windowEndTime) break;
                if (sm.startTime > windowStartTime && sm.startTime < windowEndTime)
                    boundaries.push_back(sm.startTime);
                if (sm.endTime > windowStartTime && sm.endTime < windowEndTime)
                    boundaries.push_back(sm.endTime);
            }
            boundaries.push_back(windowEndTime);
            std::sort(boundaries.begin(), boundaries.end());

            for (size_t b = 0; b + 1 < boundaries.size(); ++b)
            {
                const double segStart = boundaries[b];
                const double segEnd   = boundaries[b + 1];
                if (segEnd <= segStart) continue;
                if (!isInSolo((segStart + segEnd) * 0.5)) continue;

                const float yTop    = highwayTop + static_cast<float>((windowEndTime - segEnd)   / totalVisibleTime) * highwayHeight;
                const float yBottom = highwayTop + static_cast<float>((windowEndTime - segStart) / totalVisibleTime) * highwayHeight;
                const float clampedTop    = std::max(yTop,    highwayTop);
                const float clampedBottom = std::min(yBottom, highwayTop + highwayHeight);
                if (clampedBottom <= clampedTop) continue;

                const float segH = clampedBottom - clampedTop;
                g.setColour(juce::Colours::cyan.withAlpha(0.85f));
                // Left rail: outer edge only
                fillWarpedRectPath(leftRailX, clampedTop, soloEdgeWidth, segH);
                // Right rail: outer edge only
                fillWarpedRectPath(rightRailX + railWidth - soloEdgeWidth, clampedTop, soloEdgeWidth, segH);
            }
        }
    }

    // Clip all content that uses pitchLaneX so the swipe animation cannot overflow
    g.saveState();
    juce::Path bgPath;
    bgPath.startNewSubPath(projectPoint(highwayLeft, highwayTop));
    bgPath.lineTo(projectPoint(highwayLeft + highwayWidth, highwayTop));
    bgPath.lineTo(projectPoint(highwayLeft + highwayWidth, highwayTop + highwayHeight));
    bgPath.lineTo(projectPoint(highwayLeft, highwayTop + highwayHeight));
    bgPath.closeSubPath();
    g.reduceClipRegion(bgPath);

    // Tint lane backgrounds by Rock Band Pro Keys color zones:
    // white key columns first, then black key overlay strips on top
    {
        const float activeLaneHeight = static_cast<float>(timeAfterPlayhead / totalVisibleTime) * highwayHeight;

        const struct { int lo; int hi; juce::Colour colour; } zoneTints[] = {
            { 48, 52, juce::Colours::red    },  // C2-E2
            { 53, 59, juce::Colours::yellow },  // F2-B2
            { 60, 64, juce::Colours::blue   },  // C3-E3
            { 65, 71, juce::Colours::green  },  // F3-B3
            { 72, 72, juce::Colours::orange },  // C4
        };
        for (const auto& zone : zoneTints)
            for (int pitch = std::max(zone.lo, displayPitchMin);
                 pitch <= std::min(zone.hi, displayPitchMax); ++pitch)
            {
                if (isPitchBlackKey(pitch)) continue;
                g.setColour(zone.colour.withAlpha(0.2f));
                fillWarpedRectPath(pitchLaneX[pitch - displayPitchMin], highwayTop, whiteKeyLaneWidth, highwayHeight);

                if (pitchActive[pitch])
                {
                    constexpr int steps = 10;
                    for (int s = 0; s < steps; ++s) {
                        float stripW = (whiteKeyLaneWidth * 0.5f) / steps;
                        float ratio = 1.0f - (float)s / steps;
                        float gradAlpha = 0.6f * ratio * std::sqrt(ratio);
                        juce::Colour glowColour = zone.colour.interpolatedWith(juce::Colours::white, 0.5f * ratio);
                        g.setColour(glowColour.withAlpha(gradAlpha));
                        fillWarpedRectPath(pitchLaneX[pitch - displayPitchMin] + stripW * s, highwayTop, stripW, activeLaneHeight);
                        fillWarpedRectPath(pitchLaneX[pitch - displayPitchMin] + whiteKeyLaneWidth - stripW * (s + 1), highwayTop, stripW, activeLaneHeight);
                    }
                }
            }
        for (const auto& zone : zoneTints)
            for (int pitch = std::max(zone.lo, displayPitchMin);
                 pitch <= std::min(zone.hi, displayPitchMax); ++pitch)
            {
                if (!isPitchBlackKey(pitch)) continue;
                if (!pitchActive[pitch]) continue;

                constexpr int steps = 10;
                for (int s = 0; s < steps; ++s) {
                    float stripW = (blackKeyLaneWidth * 0.5f) / steps;
                    float ratio = 1.0f - (float)s / steps;
                    float gradAlpha = 0.6f * ratio * std::sqrt(ratio);
                    juce::Colour glowColour = zone.colour.darker().interpolatedWith(juce::Colours::white, 0.5f * ratio);
                    g.setColour(glowColour.withAlpha(gradAlpha));
                    fillWarpedRectPath(pitchLaneX[pitch - displayPitchMin] + stripW * s, highwayTop, stripW, activeLaneHeight);
                    fillWarpedRectPath(pitchLaneX[pitch - displayPitchMin] + blackKeyLaneWidth - stripW * (s + 1), highwayTop, stripW, activeLaneHeight);
                }
            }
        for (const auto& zone : zoneTints)
        {
            if (leftEdgePitch  >= zone.lo && leftEdgePitch  <= zone.hi && pitchActive[leftEdgePitch]) 
            {
                constexpr int steps = 10;
                for (int s = 0; s < steps; ++s) {
                    float stripW = (blackKeyLaneWidth * 0.5f) / steps;
                    float ratio = 1.0f - (float)s / steps;
                    float gradAlpha = 0.6f * ratio * std::sqrt(ratio);
                    juce::Colour glowColour = zone.colour.darker().interpolatedWith(juce::Colours::white, 0.5f * ratio);
                    g.setColour(glowColour.withAlpha(gradAlpha));
                    fillWarpedRectPath(leftEdgeX + stripW * s, highwayTop, stripW, activeLaneHeight);
                    fillWarpedRectPath(leftEdgeX + blackKeyLaneWidth - stripW * (s + 1), highwayTop, stripW, activeLaneHeight);
                }
            }
            if (rightEdgePitch >= zone.lo && rightEdgePitch <= zone.hi && pitchActive[rightEdgePitch]) 
            {
                constexpr int steps = 10;
                for (int s = 0; s < steps; ++s) {
                    float stripW = (blackKeyLaneWidth * 0.5f) / steps;
                    float ratio = 1.0f - (float)s / steps;
                    float gradAlpha = 0.6f * ratio * std::sqrt(ratio);
                    juce::Colour glowColour = zone.colour.darker().interpolatedWith(juce::Colours::white, 0.5f * ratio);
                    g.setColour(glowColour.withAlpha(gradAlpha));
                    fillWarpedRectPath(rightEdgeX + stripW * s, highwayTop, stripW, activeLaneHeight);
                    fillWarpedRectPath(rightEdgeX + blackKeyLaneWidth - stripW * (s + 1), highwayTop, stripW, activeLaneHeight);
                }
            }
        }
    }

    // Draw lane separators
    for (int i = 0; i < numLanes; ++i)
    {
        const int pitch = displayPitchMin + i;
        if (isPitchBlackKey(pitch)) continue;
        g.setColour((pitch % 12 == 0) ? juce::Colours::grey.withAlpha(0.7f)
                                      : juce::Colours::darkgrey.withAlpha(0.5f));
        auto p1 = projectPoint(pitchLaneX[i], highwayTop);
        auto p2 = projectPoint(pitchLaneX[i], highwayTop + highwayHeight);
        g.drawLine(p1.x, p1.y, p2.x, p2.y, 1.0f);
    }
    g.setColour(juce::Colours::darkgrey.withAlpha(0.5f));
    {
        auto p1 = projectPoint(highwayLeft + highwayWidth, highwayTop);
        auto p2 = projectPoint(highwayLeft + highwayWidth, highwayTop + highwayHeight);
        g.drawLine(p1.x, p1.y, p2.x, p2.y, 1.0f);
    }

    // Draw beat grid: bar lines, quarter notes, and eighth notes
    if (audioProcessor.TimeMap2_timeToBeats && audioProcessor.TimeMap2_beatsToTime)
    {
        double startBeat = 0.0;
        audioProcessor.TimeMap2_timeToBeats(nullptr, windowStartTime, nullptr, nullptr, &startBeat, nullptr);

        // Step in half-beat increments to draw eighth-note lines
        const double firstHalfBeat = std::ceil(startBeat * 2.0 - 1e-6) / 2.0;
        int prevMeasure = -1;
        {
            const double prevQuarterBeat = std::floor(firstHalfBeat - 1e-6);
            if (prevQuarterBeat >= 0.0)
            {
                const double prevBeatTime = audioProcessor.TimeMap2_beatsToTime(nullptr, prevQuarterBeat, nullptr);
                audioProcessor.TimeMap2_timeToBeats(nullptr, prevBeatTime, &prevMeasure, nullptr, nullptr, nullptr);
            }
        }
        for (double halfBeat = firstHalfBeat; halfBeat < firstHalfBeat + 2000.0; halfBeat += 0.5)
        {
            const double beatTime = audioProcessor.TimeMap2_beatsToTime(nullptr, halfBeat, nullptr);
            if (beatTime > windowEndTime) break;

            const bool isQuarterBeat = std::abs(halfBeat - std::round(halfBeat)) < 1e-6;

            bool isBarLine = false;
            if (isQuarterBeat)
            {
                int measure = 0;
                audioProcessor.TimeMap2_timeToBeats(nullptr, beatTime, &measure, nullptr, nullptr, nullptr);
                isBarLine = (prevMeasure >= 0 && measure != prevMeasure);
                prevMeasure = measure;
            }

            if (beatTime < windowStartTime) continue;

            const float y = highwayTop + static_cast<float>((windowEndTime - beatTime) / totalVisibleTime) * highwayHeight;
            const float s = getScale(y);

            if (isBarLine)
            {
                g.setColour(juce::Colours::white.withAlpha(0.4f));
                auto p1 = projectPoint(highwayLeft, y);
                auto p2 = projectPoint(highwayLeft + highwayWidth, y);
                g.drawLine(p1.x, p1.y, p2.x, p2.y, 10.0f * s);
            }
            else if (isQuarterBeat)
            {
                g.setColour(juce::Colours::white.withAlpha(0.4f));
                auto p1 = projectPoint(highwayLeft, y);
                auto p2 = projectPoint(highwayLeft + highwayWidth, y);
                g.drawLine(p1.x, p1.y, p2.x, p2.y, 3.0f * s);
            }
            else
            {
                g.setColour(juce::Colours::white.withAlpha(0.15f));
                auto p1 = projectPoint(highwayLeft, y);
                auto p2 = projectPoint(highwayLeft + highwayWidth, y);
                g.drawLine(p1.x, p1.y, p2.x, p2.y, 2.0f * s);
            }
        }
    }
    else
    {
        // Fallback: 0.25-second grid (eighth notes at 120 BPM) when REAPER tempo API is unavailable
        const double firstLine = std::floor(windowStartTime * 4.0 + 1.0) / 4.0;
        for (double t = firstLine; t <= windowEndTime; t += 0.25)
        {
            const float y = highwayTop + static_cast<float>((windowEndTime - t) / totalVisibleTime) * highwayHeight;
            const bool isQuarter = std::abs(t - std::round(t * 2.0) / 2.0) < 1e-6;
            g.setColour(juce::Colours::white.withAlpha(isQuarter ? 0.4f : 0.15f));
            auto p1 = projectPoint(highwayLeft, y);
            auto p2 = projectPoint(highwayLeft + highwayWidth, y);
            g.drawLine(p1.x, p1.y, p2.x, p2.y, (isQuarter ? 3.0f : 2.0f) * getScale(y));
        }
    }

    // Draw piano keyboard strip at the playhead position
    const float playheadY = highwayTop + static_cast<float>(timeAfterPlayhead / totalVisibleTime) * highwayHeight;

    const float whiteKeyH = highwayHeight * 0.058f;
    const float blackKeyH = whiteKeyH * 0.6f;
    const float keyboardTop = playheadY;

    // Range shift preview indicators
    if (nextShift != nullptr)
    {
        // Spatial dimming: dark overlay on lanes being removed for each visible
        // upcoming range shift. Each shift owns the segment from its Y position
        // up to the next shift's Y (or the top of the highway).
        {
            std::vector<const RangeShift*> upcomingShifts;
            for (const auto& rs : audioProcessor.rangeShifts)
            {
                if (rs.time <= currentTime) continue;
                upcomingShifts.push_back(&rs);
            }

            for (size_t s = 0; s < upcomingShifts.size(); ++s)
            {
                const RangeShift* shift = upcomingShifts[s];
                const float rawY = highwayTop + static_cast<float>((windowEndTime - shift->time) / totalVisibleTime) * highwayHeight;
                const float segBottom = std::max(highwayTop, std::min(rawY, playheadY));

                float segTop = highwayTop;
                if (s + 1 < upcomingShifts.size())
                {
                    const float nextRawY = highwayTop + static_cast<float>((windowEndTime - upcomingShifts[s + 1]->time) / totalVisibleTime) * highwayHeight;
                    segTop = std::max(highwayTop, std::min(nextRawY, playheadY));
                }

                if (segBottom <= segTop) continue;

                constexpr float fadeHeight = 25.0f;
                const float fadeStart = std::max(segTop, segBottom - fadeHeight);

                for (int pitch = displayPitchMin; pitch <= displayPitchMax; ++pitch)
                {
                    if (pitch >= shift->pitchMin && pitch <= shift->pitchMax)
                        continue;

                    const int idx = pitch - displayPitchMin;
                    const bool isBlack = isPitchBlackKey(pitch);
                    const float lw = isBlack ? blackKeyLaneWidth : whiteKeyLaneWidth;
                    const float lx = pitchLaneX[idx];

                    // Solid dim from top of segment to start of fade
                    if (fadeStart > segTop)
                    {
                        g.setColour(juce::Colours::black.withAlpha(0.9f));
                        fillWarpedRectPath(lx, segTop, lw, fadeStart - segTop);
                    }

                    // Gradient fade from solid dim to transparent
                    if (segBottom > fadeStart)
                    {
                        float projFadeStartY = projectPoint(lx, fadeStart).y;
                        float projSegBottomY = projectPoint(lx, segBottom).y;
                        juce::ColourGradient grad(
                            juce::Colours::black.withAlpha(0.9f), 0.0f, projFadeStartY,
                            juce::Colours::black.withAlpha(0.0f),  0.0f, projSegBottomY,
                            false);
                        g.setGradientFill(grad);
                        fillWarpedRectPath(lx, fadeStart, lw, segBottom - fadeStart);
                    }
                }

                // Redraw lane dividers over the dimmed region so they remain visible
                for (int i = 0; i < numLanes; ++i)
                {
                    const int pitch = displayPitchMin + i;
                    if (isPitchBlackKey(pitch)) continue;
                    g.setColour((pitch % 12 == 0) ? juce::Colours::grey.withAlpha(0.7f)
                                                  : juce::Colours::darkgrey.withAlpha(0.5f));
                    auto p1 = projectPoint(pitchLaneX[i], segTop);
                    auto p2 = projectPoint(pitchLaneX[i], segBottom);
                    g.drawLine(p1.x, p1.y, p2.x, p2.y, 1.0f);
                }
                g.setColour(juce::Colours::darkgrey.withAlpha(0.5f));
                {
                    auto p1 = projectPoint(highwayLeft + highwayWidth, segTop);
                    auto p2 = projectPoint(highwayLeft + highwayWidth, segBottom);
                    g.drawLine(p1.x, p1.y, p2.x, p2.y, 1.0f);
                }
            }
        }

        // Directional chevron arrows: groups of three for each upcoming shift.
        // A 2-beat gap from any preceding shift is enforced; the beat at the
        // shift marker itself (b == 0) is always drawn regardless of the gap.
        {
            const float chevW = whiteKeyLaneWidth * 0.9f;
            const float chevH = whiteKeyLaneWidth * 0.3f;
            const float chevGap = whiteKeyLaneWidth * 0.2f;
            const float chevThickness = whiteKeyLaneWidth * 0.2f;
            const float groupWidth = 3.0f * chevW + 2.0f * chevGap;

            auto drawChevronGroup = [&](float centreY, bool shiftGoingLeft, float groupX)
            {
                g.setColour((shiftGoingLeft ? juce::Colours::yellow : juce::Colours::lightskyblue).withAlpha(0.9f));

                for (int a = 0; a < 3; ++a)
                {
                    const float cx = groupX + a * (chevW + chevGap);
                    const float cy = centreY - chevH * 0.5f;
                    juce::Path chev;
                    if (shiftGoingLeft)
                    {
                        chev.startNewSubPath(projectPoint(cx + chevW, cy));
                        chev.lineTo(projectPoint(cx + chevW - chevThickness, cy));
                        chev.lineTo(projectPoint(cx, cy + chevH * 0.5f));
                        chev.lineTo(projectPoint(cx + chevW - chevThickness, cy + chevH));
                        chev.lineTo(projectPoint(cx + chevW, cy + chevH));
                        chev.lineTo(projectPoint(cx + chevThickness, cy + chevH * 0.5f));
                    }
                    else
                    {
                        chev.startNewSubPath(projectPoint(cx, cy));
                        chev.lineTo(projectPoint(cx + chevThickness, cy));
                        chev.lineTo(projectPoint(cx + chevW, cy + chevH * 0.5f));
                        chev.lineTo(projectPoint(cx + chevThickness, cy + chevH));
                        chev.lineTo(projectPoint(cx, cy + chevH));
                        chev.lineTo(projectPoint(cx + chevW - chevThickness, cy + chevH * 0.5f));
                    }
                    chev.closeSubPath();
                    g.fillPath(chev);
                }
            };

            if (audioProcessor.TimeMap2_timeToBeats && audioProcessor.TimeMap2_beatsToTime)
            {
                // Seed gap rule from the last completed shift
                double prevEventBeat = -1e9;
                if (active != nullptr)
                    audioProcessor.TimeMap2_timeToBeats(nullptr, active->time, nullptr, nullptr, &prevEventBeat, nullptr);

                int prevPitchMin = displayPitchMin;
                for (const auto& rs : audioProcessor.rangeShifts)
                {
                    if (rs.time <= currentTime) { prevPitchMin = rs.pitchMin; continue; }

                    if (rs.pitchMin != prevPitchMin)
                    {
                        const bool chevGoingLeft = rs.pitchMin < prevPitchMin;
                        const int incomingVisMin = std::max(displayPitchMin, rs.pitchMin);
                        const int incomingVisMax = std::min(displayPitchMax, rs.pitchMax);
                        double shiftBeat = 0.0;
                        audioProcessor.TimeMap2_timeToBeats(nullptr, rs.time, nullptr, nullptr, &shiftBeat, nullptr);
                        const double shiftBeatFloor = std::floor(shiftBeat + 1e-6);
                        const double earliestBeat = prevEventBeat + 3.0;

                        if (incomingVisMin <= incomingVisMax)
                        {
                            const float maxLaneW = isPitchBlackKey(incomingVisMax) ? blackKeyLaneWidth : whiteKeyLaneWidth;
                            const float groupX = chevGoingLeft
                                ? pitchLaneX[incomingVisMin - displayPitchMin] + 4.0f
                                : pitchLaneX[incomingVisMax - displayPitchMin] + maxLaneW - groupWidth - 4.0f;

                            for (int b = 0; b < 4; ++b)
                            {
                                const double beatPos = shiftBeatFloor - b;
                                if (b != 0 && beatPos < earliestBeat) continue;
                                const double beatTime = audioProcessor.TimeMap2_beatsToTime(nullptr, beatPos, nullptr);
                                if (beatTime < windowStartTime || beatTime > windowEndTime) continue;
                                const float beatY = highwayTop + static_cast<float>((windowEndTime - beatTime) / totalVisibleTime) * highwayHeight;
                                if (beatY < highwayTop || beatY > highwayTop + highwayHeight) continue;
                                drawChevronGroup(beatY, chevGoingLeft, groupX);
                            }
                        }

                        prevEventBeat = shiftBeat;
                    }
                    prevPitchMin = rs.pitchMin;
                }
            }
            else
            {
                // Fallback: approximate with 120 BPM (0.5 sec/beat)
                constexpr double secPerBeat = 0.5;
                double prevEventTime = (active != nullptr) ? active->time : -1e9;
                int prevPitchMin = displayPitchMin;
                for (const auto& rs : audioProcessor.rangeShifts)
                {
                    if (rs.time <= currentTime) { prevPitchMin = rs.pitchMin; continue; }

                    if (rs.pitchMin != prevPitchMin)
                    {
                        const bool chevGoingLeft = rs.pitchMin < prevPitchMin;
                        const int incomingVisMin = std::max(displayPitchMin, rs.pitchMin);
                        const int incomingVisMax = std::min(displayPitchMax, rs.pitchMax);
                        const double shiftTimeFloor = std::floor(rs.time / secPerBeat + 1e-6) * secPerBeat;
                        const double earliestTime = prevEventTime + 3.0 * secPerBeat;

                        if (incomingVisMin <= incomingVisMax)
                        {
                            const float maxLaneW = isPitchBlackKey(incomingVisMax) ? blackKeyLaneWidth : whiteKeyLaneWidth;
                            const float groupX = chevGoingLeft
                                ? pitchLaneX[incomingVisMin - displayPitchMin] + 4.0f
                                : pitchLaneX[incomingVisMax - displayPitchMin] + maxLaneW - groupWidth - 4.0f;

                            for (int b = 0; b < 4; ++b)
                            {
                                const double beatTime = shiftTimeFloor - b * secPerBeat;
                                if (b != 0 && beatTime < earliestTime) continue;
                                if (beatTime < windowStartTime || beatTime > windowEndTime) continue;
                                const float beatY = highwayTop + static_cast<float>((windowEndTime - beatTime) / totalVisibleTime) * highwayHeight;
                                if (beatY < highwayTop || beatY > highwayTop + highwayHeight) continue;
                                drawChevronGroup(beatY, chevGoingLeft, groupX);
                            }
                        }

                        prevEventTime = rs.time;
                    }
                    prevPitchMin = rs.pitchMin;
                }
            }
        }
    }

    auto getZoneColour = [](int pitch) -> juce::Colour {
        if (pitch >= 48 && pitch <= 52) return juce::Colours::red;
        if (pitch >= 53 && pitch <= 59) return juce::Colours::yellow;
        if (pitch >= 60 && pitch <= 64) return juce::Colours::blue;
        if (pitch >= 65 && pitch <= 71) return juce::Colours::green;
        if (pitch == 72)                return juce::Colours::orange;
        return juce::Colours::grey;
    };

    // Draw trill lanes behind the strikeline
    for (const auto& trill : audioProcessor.trillMarkers)
    {
        if (trill.endTime < windowStartTime || trill.startTime > windowEndTime) continue;

        // Find distinct pitches of notes whose start times fall within this trill marker
        std::vector<int> trillPitches;
        for (const auto& note : audioProcessor.parsedNotes)
        {
            if (note.startTime < trill.startTime - 0.001) continue;
            if (note.startTime > trill.endTime + 0.001) break;
            if (note.pitch < 48 || note.pitch > 72) continue;
            if (std::find(trillPitches.begin(), trillPitches.end(), note.pitch) == trillPitches.end())
                trillPitches.push_back(note.pitch);
        }

        const bool isValid = (trillPitches.size() == 2);

        const float trillYTop    = highwayTop + static_cast<float>((windowEndTime - trill.endTime)   / totalVisibleTime) * highwayHeight;
        const float trillYBottom = highwayTop + static_cast<float>((windowEndTime - trill.startTime) / totalVisibleTime) * highwayHeight;
        const float clampedTop    = std::max(trillYTop, highwayTop);
        const float clampedBottom = std::min(trillYBottom, highwayTop + highwayHeight);
        if (clampedBottom <= clampedTop) continue;
        const float trillH = clampedBottom - clampedTop;

        if (isValid)
        {
            for (int pitch : trillPitches)
            {
                if (pitch < displayPitchMin || pitch > displayPitchMax) continue;
                const int idx = pitch - displayPitchMin;
                const bool isBlack = isPitchBlackKey(pitch);
                const float laneW = isBlack ? blackKeyLaneWidth : whiteKeyLaneWidth;
                const float laneX = pitchLaneX[idx];
                const juce::Colour zoneCol = getZoneColour(pitch);
                constexpr float edgeW = 3.0f;

                // Dark centre fill
                g.setColour(juce::Colours::black.withAlpha(0.7f));
                fillWarpedRectPath(laneX + edgeW, clampedTop, laneW - edgeW * 2.0f, trillH);

                // Coloured rounded edges (left and right strips)
                g.setColour(zoneCol.withAlpha(0.7f));
                fillWarpedRectPath(laneX, clampedTop, edgeW, trillH);
                fillWarpedRectPath(laneX + laneW - edgeW, clampedTop, edgeW, trillH);
            }
        }
        else
        {
            // Invalid trill: draw red-stroked lanes under all pitches in range
            for (int pitch = displayPitchMin; pitch <= displayPitchMax; ++pitch)
            {
                const int idx = pitch - displayPitchMin;
                const bool isBlack = isPitchBlackKey(pitch);
                const float laneW = isBlack ? blackKeyLaneWidth : whiteKeyLaneWidth;
                const float laneX = pitchLaneX[idx];
                constexpr float edgeW = 3.0f;

                // Dark centre fill
                g.setColour(juce::Colours::black.withAlpha(0.6f));
                fillWarpedRectPath(laneX + edgeW, clampedTop, laneW - edgeW * 2.0f, trillH);

                // Red edges to indicate invalid
                g.setColour(juce::Colours::red.withAlpha(0.7f));
                fillWarpedRectPath(laneX, clampedTop, edgeW, trillH);
                fillWarpedRectPath(laneX + laneW - edgeW, clampedTop, edgeW, trillH);
            }
        }
    }

    // Keyboard background strip
    g.setColour(juce::Colour(0xff0d0d0d));
    fillWarpedRectPath(highwayLeft, keyboardTop, highwayWidth, whiteKeyH);

    // White keys (full strip height)
    for (int pitch = displayPitchMin; pitch <= displayPitchMax; ++pitch)
    {
        if (isPitchBlackKey(pitch)) continue;
        const float kx = pitchLaneX[pitch - displayPitchMin];
        g.setColour(pitchActive[pitch] ? juce::Colours::white : juce::Colour(0xffcccccc));
        fillWarpedRectPath(kx + 1.0f, keyboardTop + 1.0f, whiteKeyLaneWidth - 2.0f, whiteKeyH - 2.0f);
    }

    // Black keys (top-aligned, shorter, overlaid on white key boundaries)
    for (int pitch = displayPitchMin; pitch <= displayPitchMax; ++pitch)
    {
        if (!isPitchBlackKey(pitch)) continue;
        const float kx = pitchLaneX[pitch - displayPitchMin];
        g.setColour(pitchActive[pitch] ? juce::Colours::white : getZoneColour(pitch).darker());
        fillWarpedRectPath(kx + 0.5f, keyboardTop, blackKeyLaneWidth - 1.0f, blackKeyH);
    }

    if (leftEdgePitch  >= 0) { g.setColour(pitchActive[leftEdgePitch] ? juce::Colours::white : getZoneColour(leftEdgePitch).darker()); fillWarpedRectPath(leftEdgeX  + 0.5f, keyboardTop, blackKeyLaneWidth - 1.0f, blackKeyH); }
    if (rightEdgePitch >= 0) { g.setColour(pitchActive[rightEdgePitch] ? juce::Colours::white : getZoneColour(rightEdgePitch).darker()); fillWarpedRectPath(rightEdgeX + 0.5f, keyboardTop, blackKeyLaneWidth - 1.0f, blackKeyH); }

    // Thin hit line
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    {
        auto p1 = projectPoint(highwayLeft, playheadY);
        auto p2 = projectPoint(highwayLeft + highwayWidth, playheadY);
        g.drawLine(p1.x, p1.y, p2.x, p2.y, 1.0f);
    }

    // Draw notes on top of range shift overlay so they are never dimmed.
    // Notes longer than a 16th note show a rounded-square head at the attack
    // plus a thin sustain tail extending up to the note's end time.

    // Binary search to find the first note that could be visible, avoiding a full
    // linear scan from the beginning of the song every frame.  We look for the
    // first note whose startTime >= windowStartTime - 30 s (generous upper bound
    // on the longest possible sustain).
    const auto& notes = audioProcessor.parsedNotes;
    const size_t firstVisibleIdx = static_cast<size_t>(std::distance(
        notes.begin(),
        std::lower_bound(notes.begin(), notes.end(), windowStartTime - 30.0,
            [](const GameNote& n, double t) { return n.startTime < t; })));

    auto noteStartsInGlissando = [&](double noteStartTime) -> bool
    {
        for (const auto& marker : audioProcessor.glissandoMarkers)
        {
            if (noteStartTime < marker.startTime)
                break;
            if (noteStartTime >= marker.startTime && noteStartTime < marker.endTime)
                return true;
        }
        return false;
    };

    auto noteStartsInOverdrive = [&](double noteStartTime) -> bool
    {
        for (const auto& marker : audioProcessor.overdriveMarkers)
        {
            if (noteStartTime < marker.startTime)
                break;
            if (noteStartTime >= marker.startTime && noteStartTime < marker.endTime)
                return true;
        }
        return false;
    };

    auto noteIsInChord = [&](const GameNote& note) -> bool
    {
        // Binary search for the note's start time, then check if any neighbour shares it
        auto it = std::lower_bound(notes.begin(), notes.end(), note.startTime - 0.001,
            [](const GameNote& n, double t) { return n.startTime < t; });
        int count = 0;
        for (; it != notes.end() && std::abs(it->startTime - note.startTime) < 0.001; ++it)
            if (++count >= 2) return true;
        return false;
    };

    auto drawNote = [&](const GameNote& note, bool isSustainPass)
    {
        const int i = note.pitch - displayPitchMin;
        const bool isBlack = isPitchBlackKey(note.pitch);
        const bool isGlissNote = noteStartsInGlissando(note.startTime);
        const bool isOverdriveNote = !isGlissNote && noteStartsInOverdrive(note.startTime);
        const float laneW = isBlack ? blackKeyLaneWidth : whiteKeyLaneWidth;
        constexpr float noteGap = 2.0f;
        const float noteX     = pitchLaneX[i] + noteGap * 0.5f;
        const float noteWidth = laneW - noteGap;
        const juce::Colour noteColour = isGlissNote
            ? (isBlack ? juce::Colours::red : juce::Colour(0xffcccccc))
            : (isBlack ? juce::Colour(0xff000000) : juce::Colour(0xffcccccc));

        const float startY        = highwayTop + static_cast<float>((windowEndTime - note.startTime) / totalVisibleTime) * highwayHeight;
        const float highwayBottom = highwayTop + highwayHeight;

        if (isSustainPass)
        {
            // Compute sustain threshold (0.299 beats, scaled by time sig denominator relative to 4)
            // so the threshold reflects the actual tempo at the note, not the playhead.
            double sustainThreshold = 0.299 * 0.5; // fallback: 0.299 beats at 120 BPM (0.5 sec/beat)
            if (audioProcessor.TimeMap2_timeToBeats && audioProcessor.TimeMap2_beatsToTime)
            {
                double noteBeat = 0.0;
                int cdenom = 4;
                audioProcessor.TimeMap2_timeToBeats(nullptr, note.startTime, nullptr, nullptr, &noteBeat, &cdenom);
                const double scaledBeats = 0.299 * (cdenom / 4.0);
                const double t0 = audioProcessor.TimeMap2_beatsToTime(nullptr, noteBeat, nullptr);
                const double t1 = audioProcessor.TimeMap2_beatsToTime(nullptr, noteBeat + scaledBeats, nullptr);
                sustainThreshold = t1 - t0;
            }

            const bool hasSustain = (note.endTime - note.startTime) >= sustainThreshold - 1e-4;

            if (hasSustain)
            {
                const float endY       = highwayTop + static_cast<float>((windowEndTime - note.endTime)   / totalVisibleTime) * highwayHeight;

                // Thin tail runs from endY down to the bottom of the gem (startY), perfectly hidden behind it
                const float tailTop    = std::max(endY,    highwayTop);
                // If note has been hit, the bottom of the tail clamps to playheadY, else startY
                const float hitClampedBottom = (note.startTime <= currentTime) ? playheadY : startY;
                const float tailBottom = std::min(hitClampedBottom,  highwayBottom);
                if (tailTop < tailBottom)
                {
                    const float uniformTailWidth = (whiteKeyLaneWidth - noteGap) * 0.2f;
                    const float tailWidth = std::min(uniformTailWidth, noteWidth);
                    const float tailX     = noteX + (noteWidth - tailWidth) * 0.5f;
                    if (isBlack)
                    {
                        const float innerTailWidth = tailWidth * 0.4f;
                        const float innerTailX = noteX + (noteWidth - innerTailWidth) * 0.5f;
                        g.setColour(juce::Colours::white.withAlpha(0.7f));
                        fillWarpedRectPath(tailX, tailTop, tailWidth, tailBottom - tailTop);
                        g.setColour(juce::Colours::black.withAlpha(0.7f));
                        fillWarpedRectPath(innerTailX, tailTop, innerTailWidth, tailBottom - tailTop);
                    }
                    else
                    {
                        g.setColour(noteColour.withAlpha(0.7f));
                        fillWarpedRectPath(tailX, tailTop, tailWidth, tailBottom - tailTop);
                    }
                }
            }
            return;
        }

        // --- Gem Pass ---
        if (note.startTime <= currentTime)
            return;

        juce::Image& gemImg = isGlissNote
            ? (isBlack ? blackErrorGemImg
                       : (noteIsInChord(note) ? whiteErrorGemImg : whiteGlissGemImg))
            : (isOverdriveNote ? (isBlack ? blackOdGemImg : whiteOdGemImg)
                               : (isBlack ? blackGemImg : whiteGemImg));
        if (gemImg.isValid())
        {
            if (startY > highwayTop && startY - 100.0f < highwayBottom) // 100.0f is generic padding check
            {
                float s = getScale(startY);
                float projBottomY = highwayTop + (startY - highwayTop) * s;
                float projX = vpX + (noteX - vpX) * s;
                float projW = noteWidth * s;

                float imgW = static_cast<float>(gemImg.getWidth());
                float imgH = static_cast<float>(gemImg.getHeight());
                float projH = projW * (imgH / imgW);
                float projTopY = projBottomY - projH + 4.0f; // Move down slightly

                g.setOpacity(1.0f);
                g.drawImage(gemImg, projX, projTopY, projW, projH,
                            0, 0, static_cast<int>(imgW), static_cast<int>(imgH));
            }
        }
        else
        {
            // Note head: square whose bottom edge sits at startY
            const float headSize = whiteKeyLaneWidth - noteGap;
            const float headTop  = startY - headSize;

            // Fallback: Draw note head clamped to highway bounds
            const float clampedHeadTop    = std::max(headTop, highwayTop);
            const float clampedHeadBottom = std::min(startY,  highwayBottom);
            if (clampedHeadBottom > clampedHeadTop)
            {
                g.setColour(noteColour);

                // Apply 3D math variable warp directly for drawing billboard notes
                float s = getScale(clampedHeadTop);
                float projX = vpX + (noteX - vpX) * s;
                float projY = highwayTop + (clampedHeadTop - highwayTop) * s;
                float projW = noteWidth * s;
                float projH = (clampedHeadBottom - clampedHeadTop) * s;

                g.fillRoundedRectangle(projX, projY, projW, projH, 3.0f * s);
            }
        }
    };

    // Draw all sustains first, so they are fully behind all gems
    for (size_t ni = firstVisibleIdx; ni < notes.size(); ++ni)
    {
        const auto& note = notes[ni];
        if (note.startTime > windowEndTime) break;
        if (note.pitch < displayPitchMin || note.pitch > displayPitchMax) continue;
        if (note.endTime < windowStartTime) continue;
        drawNote(note, true);
    }

    // Chord bar pass: connect simultaneous notes with a thin horizontal bar.
    {
        constexpr float noteGap  = 2.0f;
        const float barH     = whiteKeyLaneWidth * 0.4f;
        const float overflow = whiteKeyLaneWidth * 0.2f;

        for (size_t j = firstVisibleIdx; j < notes.size(); )
        {
            const double t = notes[j].startTime;
            if (t > windowEndTime) break;

            // Find the end of this startTime group, use a small epsilon for start time equality
            size_t k = j + 1;
            while (k < notes.size() &&
                   std::abs(notes[k].startTime - t) < 0.001) ++k;

            if (k - j >= 2)
            {
                if (t <= currentTime)
                {
                    j = k;
                    continue;
                }

                // Skip chords with any out-of-range notes (drawn in unclipped pass)
                bool hasOutOfRange = false;
                for (size_t n = j; n < k; ++n)
                {
                    const int pitch = notes[n].pitch;
                    if (pitch >= 48 && pitch <= 72 && (pitch < displayPitchMin || pitch > displayPitchMax))
                    { hasOutOfRange = true; break; }
                }
                if (hasOutOfRange) { j = k; continue; }

                float minX = std::numeric_limits<float>::max();
                float maxX = std::numeric_limits<float>::lowest();
                int validNotesInChord = 0;

                for (size_t n = j; n < k; ++n)
                {
                    const int pitch = notes[n].pitch;
                    if (pitch < 48 || pitch > 72) continue; // Only count notes in valid Pro Keys range (48-72)
                    if (pitch < displayPitchMin || pitch > displayPitchMax) continue;
                    if (notes[n].endTime < windowStartTime) continue;

                    const int idx = pitch - displayPitchMin;
                    const bool isBlack = isPitchBlackKey(pitch);
                    const float laneW = isBlack ? blackKeyLaneWidth : whiteKeyLaneWidth;
                    const float nx = pitchLaneX[idx] + noteGap * 0.5f;
                    minX = std::min(minX, nx);
                    maxX = std::max(maxX, nx + laneW - noteGap);
                    validNotesInChord++;
                }

                if (validNotesInChord >= 2 && maxX > minX)
                {
                    minX -= overflow;
                    maxX += overflow;
                    const float startY = highwayTop + static_cast<float>((windowEndTime - t) / totalVisibleTime) * highwayHeight;
                    const float barTop = startY - barH * 0.5f + 4.0f;
                    if (barTop + barH >= highwayTop && barTop <= highwayTop + highwayHeight)
                    {
                        g.setColour(juce::Colours::white.withAlpha(0.5f));
                        float s = getScale(barTop);
                        float pMinX = vpX + (minX - vpX) * s;
                        float pMaxX = vpX + (maxX - vpX) * s;
                        float pBarTop = highwayTop + (barTop - highwayTop) * s;
                        float pBarH = barH * s;
                        g.fillRoundedRectangle(pMinX, pBarTop, pMaxX - pMinX, pBarH, pBarH * 0.5f);
                    }
                }
            }

            j = k;
        }
    }

    g.restoreState(); // Remove highway clip so edge gems are not cut off

    // Clip at the top of the highway only (not sides) so gems don't poke above
    g.saveState();
    g.reduceClipRegion(0, static_cast<int>(highwayTop), getWidth(), getHeight() - static_cast<int>(highwayTop));

    // Draw gems depth-ordered: newest (top/far) first, oldest (bottom/near) last
    // so that closer notes layer on top of farther ones.
    for (size_t ri = notes.size(); ri > firstVisibleIdx; )
    {
        --ri;
        const auto& note = notes[ri];
        if (note.endTime < windowStartTime) break;
        if (note.pitch < displayPitchMin || note.pitch > displayPitchMax) continue;
        if (note.startTime > windowEndTime) continue;
        drawNote(note, false);
    }

    // Draw smash animations ON TOP of everything else
    for (int pitch = displayPitchMin; pitch <= displayPitchMax; ++pitch)
    {
        for (int frame : pitchSmashFrames[pitch])
        {
            juce::Image& img = smashImgs[frame];
            if (img.isValid())
            {
                float kx = pitchLaneX[pitch - displayPitchMin];
                float s = getScale(keyboardTop);
                float projX = vpX + (kx - vpX) * s;
                float projY = highwayTop + (keyboardTop - highwayTop) * s;

                // Use whiteKeyLaneWidth consistently for the smash animation base size
                float laneW = whiteKeyLaneWidth;
                float projW = laneW * s;

                float aspect = static_cast<float>(img.getHeight()) / img.getWidth();
                float imgProjW = projW * 2.5f; 
                float imgProjH = imgProjW * aspect;

                // Center around the actual lane visual width
                float actualLaneW = (isPitchBlackKey(pitch) ? blackKeyLaneWidth : whiteKeyLaneWidth) * s;
                g.setOpacity(1.0f);
                g.drawImage(img, projX + actualLaneW * 0.5f - imgProjW * 0.5f, projY - imgProjH * 0.5f, imgProjW, imgProjH,
                            0, 0, img.getWidth(), img.getHeight());
            }
        }

        if (pitchSustaining[pitch])
        {
            juce::Image& img = smashFlareImg;
            if (img.isValid())
            {
                float kx = pitchLaneX[pitch - displayPitchMin];
                float s = getScale(keyboardTop);
                float projX = vpX + (kx - vpX) * s;
                float projY = highwayTop + (keyboardTop - highwayTop) * s;

                // Use whiteKeyLaneWidth consistently for base size to keep flare uniform
                float laneW = whiteKeyLaneWidth;
                float projW = laneW * s;

                float aspect = static_cast<float>(img.getHeight()) / img.getWidth();
                float imgProjW = projW * 2.0f; // slightly smaller than full smash
                float imgProjH = imgProjW * aspect;

                // Center around the actual lane visual width
                float actualLaneW = (isPitchBlackKey(pitch) ? blackKeyLaneWidth : whiteKeyLaneWidth) * s;
                g.setOpacity(1.0f);
                g.drawImage(img, projX + actualLaneW * 0.5f - imgProjW * 0.5f, projY - imgProjH * 0.5f, imgProjW, imgProjH,
                            0, 0, img.getWidth(), img.getHeight());
            }
        }
    }

    // Chord bar pass for chords containing out-of-range notes (drawn unclipped)
    {
        constexpr float noteGap  = 2.0f;
        const float barH     = whiteKeyLaneWidth * 0.6f;
        const float overflow = whiteKeyLaneWidth * 0.45f;

        for (size_t j = firstVisibleIdx; j < notes.size(); )
        {
            const double t = notes[j].startTime;
            if (t > windowEndTime) break;

            size_t k = j + 1;
            while (k < notes.size() &&
                   std::abs(notes[k].startTime - t) < 0.001) ++k;

            if (k - j >= 2)
            {
                if (t <= currentTime)
                {
                    j = k;
                    continue;
                }

                // Only handle chords with at least one out-of-range note
                bool hasOutOfRange = false;
                for (size_t n = j; n < k; ++n)
                {
                    const int pitch = notes[n].pitch;
                    if (pitch >= 48 && pitch <= 72 && (pitch < displayPitchMin || pitch > displayPitchMax))
                    { hasOutOfRange = true; break; }
                }
                if (!hasOutOfRange) { j = k; continue; }

                float minX = std::numeric_limits<float>::max();
                float maxX = std::numeric_limits<float>::lowest();
                int validNotesInChord = 0;

                for (size_t n = j; n < k; ++n)
                {
                    const int pitch = notes[n].pitch;
                    if (pitch < 48 || pitch > 72) continue;
                    if (notes[n].endTime < windowStartTime) continue;

                    const int allIdx = pitch - 48;
                    const bool isBlack = isPitchBlackKey(pitch);
                    const float laneW = isBlack ? blackKeyLaneWidth : whiteKeyLaneWidth;
                    const float nx = allPitchX[allIdx] + noteGap * 0.5f;
                    minX = std::min(minX, nx);
                    maxX = std::max(maxX, nx + laneW - noteGap);
                    validNotesInChord++;
                }

                if (validNotesInChord >= 2 && maxX > minX)
                {
                    minX -= overflow;
                    maxX += overflow;
                    const float startY = highwayTop + static_cast<float>((windowEndTime - t) / totalVisibleTime) * highwayHeight;
                    const float barTop = startY - barH * 0.5f + 4.0f;
                    if (barTop + barH >= highwayTop && barTop <= highwayTop + highwayHeight)
                    {
                        g.setColour(juce::Colours::white.withAlpha(0.6f));
                        float s = getScale(barTop);
                        float pMinX = vpX + (minX - vpX) * s;
                        float pMaxX = vpX + (maxX - vpX) * s;
                        float pBarTop = highwayTop + (barTop - highwayTop) * s;
                        float pBarH = barH * s;
                        g.fillRoundedRectangle(pMinX, pBarTop, pMaxX - pMinX, pBarH, pBarH * 0.5f);
                    }
                }
            }

            j = k;
        }
    }

    // Draw out-of-range error notes (pitches 48-72 outside current display range)
    {
        auto drawOutOfRangeNote = [&](const GameNote& note, bool isSustainPass)
        {
            const int allIdx = note.pitch - 48;
            const bool isBlack = isPitchBlackKey(note.pitch);
            const float laneW = isBlack ? blackKeyLaneWidth : whiteKeyLaneWidth;
            constexpr float noteGap = 2.0f;
            const float noteX = allPitchX[allIdx] + noteGap * 0.5f;
            const float noteWidth = laneW - noteGap;
            const float startY = highwayTop + static_cast<float>((windowEndTime - note.startTime) / totalVisibleTime) * highwayHeight;
            const float highwayBottom = highwayTop + highwayHeight;

            if (isSustainPass)
            {
                double sustainThreshold = 0.299 * 0.5;
                if (audioProcessor.TimeMap2_timeToBeats && audioProcessor.TimeMap2_beatsToTime)
                {
                    double noteBeat = 0.0;
                    int cdenom = 4;
                    audioProcessor.TimeMap2_timeToBeats(nullptr, note.startTime, nullptr, nullptr, &noteBeat, &cdenom);
                    const double scaledBeats = 0.299 * (cdenom / 4.0);
                    const double t0 = audioProcessor.TimeMap2_beatsToTime(nullptr, noteBeat, nullptr);
                    const double t1 = audioProcessor.TimeMap2_beatsToTime(nullptr, noteBeat + scaledBeats, nullptr);
                    sustainThreshold = t1 - t0;
                }
                const bool hasSustain = (note.endTime - note.startTime) >= sustainThreshold - 1e-4;
                if (hasSustain)
                {
                    const float endY = highwayTop + static_cast<float>((windowEndTime - note.endTime) / totalVisibleTime) * highwayHeight;
                    const float tailTop = std::max(endY, highwayTop);
                    const float tailBottom = std::min(startY, highwayBottom);
                    if (tailTop < tailBottom)
                    {
                        const float uniformTailWidth = (whiteKeyLaneWidth - noteGap) * 0.2f;
                        const float tailWidth = std::min(uniformTailWidth, noteWidth);
                        const float tailX = noteX + (noteWidth - tailWidth) * 0.5f;
                        if (isBlack)
                        {
                            const float innerTailWidth = tailWidth * 0.4f;
                            const float innerTailX = noteX + (noteWidth - innerTailWidth) * 0.5f;
                            g.setColour(juce::Colours::white.withAlpha(0.7f));
                            fillWarpedRectPath(tailX, tailTop, tailWidth, tailBottom - tailTop);
                            g.setColour(juce::Colours::black.withAlpha(0.7f));
                            fillWarpedRectPath(innerTailX, tailTop, innerTailWidth, tailBottom - tailTop);
                        }
                        else
                        {
                            g.setColour(juce::Colour(0xffcccccc).withAlpha(0.7f));
                            fillWarpedRectPath(tailX, tailTop, tailWidth, tailBottom - tailTop);
                        }
                    }
                }
                return;
            }

            // Gem pass - error notes disappear at the strikeline
            if (note.startTime <= currentTime)
                return;

            juce::Image& gemImg = isBlack ? blackErrorGemImg : whiteErrorGemImg;
            if (gemImg.isValid())
            {
                if (startY > highwayTop && startY - 100.0f < highwayBottom)
                {
                    float s = getScale(startY);
                    float projBottomY = highwayTop + (startY - highwayTop) * s;
                    float projX = vpX + (noteX - vpX) * s;
                    float projW = noteWidth * s;

                    float imgW = static_cast<float>(gemImg.getWidth());
                    float imgH = static_cast<float>(gemImg.getHeight());
                    float projH = projW * (imgH / imgW);
                    float projTopY = projBottomY - projH + 4.0f;

                    g.setOpacity(1.0f);
                    g.drawImage(gemImg, projX, projTopY, projW, projH,
                                0, 0, static_cast<int>(imgW), static_cast<int>(imgH));
                }
            }
        };

        // Sustain pass
        for (size_t ni = firstVisibleIdx; ni < notes.size(); ++ni)
        {
            const auto& note = notes[ni];
            if (note.startTime > windowEndTime) break;
            if (note.pitch < 48 || note.pitch > 72) continue;
            if (note.pitch >= displayPitchMin && note.pitch <= displayPitchMax) continue;
            if (note.endTime < windowStartTime) continue;
            drawOutOfRangeNote(note, true);
        }

        // Gem pass
        for (size_t ni = firstVisibleIdx; ni < notes.size(); ++ni)
        {
            const auto& note = notes[ni];
            if (note.startTime > windowEndTime) break;
            if (note.pitch < 48 || note.pitch > 72) continue;
            if (note.pitch >= displayPitchMin && note.pitch <= displayPitchMax) continue;
            if (note.endTime < windowStartTime) continue;
            drawOutOfRangeNote(note, false);
        }
    }

    g.restoreState(); // Remove top-only clip

    // Fade highway into the background near the top (applies to all content)
    {
        const float topFadeHeight = highwayHeight * 0.15f;
        const float fadeEndY = std::min(highwayTop + topFadeHeight, highwayTop + highwayHeight);
        const float projTopY = projectPoint(highwayLeft, highwayTop).y;
        const float projFadeEndY = projectPoint(highwayLeft, fadeEndY).y;
        juce::ColourGradient fadeGrad(
            juce::Colours::black.withAlpha(0.9f), 0.0f, projTopY,
            juce::Colours::black.withAlpha(0.0f), 0.0f, projFadeEndY,
            false);
        g.setGradientFill(fadeGrad);
        g.fillRect(0.0f, projTopY, static_cast<float>(getWidth()), projFadeEndY - projTopY);

        // Solid black above the projected highway top to fully hide any content above the fade
        if (projTopY > 0.0f)
        {
            g.setColour(juce::Colours::black);
            g.fillRect(0.0f, 0.0f, static_cast<float>(getWidth()), projTopY);
        }
    }

    // Watermark: bottom-left corner, always on top of all other content
    {
        const juce::String watermark = juce::String(JucePlugin_Name)
                                     + " v" + juce::String(JucePlugin_VersionString)
                                     + " by " + juce::String(JucePlugin_Manufacturer);
        g.setFont(juce::Font(11.0f));
        g.setColour(juce::Colours::white.withAlpha(0.25f));
        const int margin = 6;
        const int textH = 14;
        g.drawText(watermark,
                   margin, getHeight() - textH - margin,
                   getWidth() - margin * 2, textH,
                   juce::Justification::bottomLeft, false);
    }
}

void ProKeysPreviewAudioProcessorEditor::resized()
{
    // Position controls in top left corner
    const int margin = 5;
    const int labelWidth = 100;
    const int difficultyLabelWidth = 55;
    const int editorWidth = 60;
    const int comboWidth = 95;
    const int height = 18;
    const int spacing = 4;

    int currentX = margin;

    // Difficulty controls
    difficultyLabel.setBounds(currentX, margin, difficultyLabelWidth, height);
    currentX += difficultyLabelWidth + 4;
    difficultyComboBox.setBounds(currentX, margin, comboWidth, height);
    currentX += comboWidth + spacing;

    // Time offset controls
    timeOffsetLabel.setBounds(currentX, margin, labelWidth, height);
    currentX += labelWidth + 4;
    timeOffsetEditor.setBounds(currentX, margin, editorWidth, height);
    currentX += editorWidth + spacing;

    // Refresh rate controls
    refreshRateLabel.setBounds(currentX, margin, labelWidth, height);
    currentX += labelWidth + 4;
    refreshRateEditor.setBounds(currentX, margin, editorWidth, height);

    // Track speed controls (second row)
    const int row2Y = margin + height + 3;
    trackSpeedLabel.setBounds(margin, row2Y, 45, height);
    trackSpeedComboBox.setBounds(margin + 45 + 4, row2Y, 85, height);
}

void ProKeysPreviewAudioProcessorEditor::timerCallback()
{
    bool nowPlaying = audioProcessor.isPlaying.load();
    if (nowPlaying && !m_wasPlayingForFetch)
        audioProcessor.invalidateMidiCache();
    m_wasPlayingForFetch = nowPlaying;

    audioProcessor.fetchMidiTake();
    repaint();
}
