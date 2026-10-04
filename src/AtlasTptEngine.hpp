// Low-cost, topology-preserving approximation of the Ripples Atlas core.
//
// This is deliberately kept separate from Atlas.cpp so that the reference
// engine and the economical engine can be compared without changing either
// implementation.  The four cells are trapezoidal (TPT) one-poles.  A
// sample's cascade is affine in the filter input, which lets us close the
// resonance loop with a bounded Newton solve instead of a
// sub-stepped ODE solve.
#pragma once

#ifndef VOSTOK_RIPPLES_STANDALONE
#include "plugin.hpp"
#endif

#include "ripples/ripples.hpp"

#include <algorithm>
#include <cmath>

struct AtlasTptEngine {
    struct Parameters {
        // These are intentionally exposed for the response-fitting harness.
        // Defaults retain the physical Ripples scaling and require no fit.
        float inputScale = 1.f;
        float outputScale = 1.f;
        float hpOutputGain = 1.f;
        float bpOutputGain = 1.f;
        float resonanceLinear = 1.f;
        float resonanceQuadratic = 0.f;
        float cutoffScale = 1.f;
        float cutoffTilt = 0.f;
        float feedbackScale = 1.f;
        // Fitted feedback tilt lets the economical discretization recover
        // the reference's stronger high-cutoff resonance without changing
        // the physical unity feedback default.
        float feedbackCutoffTilt = 0.f;
    };

    AtlasTptEngine() { setSampleRate(1.f); }

    void setParameters(const Parameters &newParameters) { parameters = newParameters; }

    void setSampleRate(float sampleRate) {
        sampleRate = std::max(sampleRate, 1.f);
        sampleTime = 1.f / sampleRate;

        // The real control amplifiers are roughly 6 kHz poles.  One base-rate
        // pole gives the same useful behaviour without Ripples' oversampling.
        const float controlPole = 6000.f;
        controlSmoothing = std::exp(-2.f * static_cast<float>(M_PI) * controlPole * sampleTime);

        const float feedforwardPole = 1.f / (2.f * static_cast<float>(M_PI) * ripples::kFeedforwardR * ripples::kFeedforwardC);
        feedforwardSmoothing = std::exp(-2.f * static_cast<float>(M_PI) * feedforwardPole * sampleTime);
        reset();
    }

    void reset() {
        stageState[0] = stageState[1] = stageState[2] = stageState[3] = 0.f;
        smoothedVOct = 0.f;
        smoothedResCurrent = 0.f;
        feedforwardLowpass = 0.f;
    }

    // Frame is intentionally the same public contract as RipplesEngine.  The
    // caller can own one instance per voice/row; no allocations occur here.
    void process(ripples::RipplesEngine::Frame &frame) {
        float vOct = (frame.freq_knob - 1.f) * ripples::kFreqKnobVoltage + frame.freq_cv + frame.fm_cv * frame.fm_knob;
        vOct = std::min(vOct, 0.f);
        const float requestedCutoff = ripples::kFreqKnobMax * std::exp2(vOct);
        const float normalizedCutoff = std::max(requestedCutoff, ripples::kFreqKnobMin) / 1000.f;
        const float correctedCutoff =
            std::clamp(requestedCutoff * parameters.cutoffScale * std::pow(normalizedCutoff, parameters.cutoffTilt),
                       ripples::kFreqKnobMin, ripples::kFreqKnobMax);
        const float correctedVOct = std::log2(correctedCutoff / ripples::kFreqKnobMax);
        smoothedVOct = controlSmoothing * smoothedVOct + (1.f - controlSmoothing) * correctedVOct;

        const float resonanceKnob = std::clamp(
            frame.res_knob * (parameters.resonanceLinear + parameters.resonanceQuadratic * frame.res_knob), 0.f, 1.25f);
        const float resonanceCurrent = VtoIConverter(ripples::kResAmpR, frame.res_cv, ripples::kResInputR,
                                                     resonanceKnob * ripples::kResKnobV, ripples::kResKnobR);
        smoothedResCurrent = controlSmoothing * smoothedResCurrent + (1.f - controlSmoothing) * resonanceCurrent;

        // Ripples adds this dither before its oversampled core to bootstrap
        // self-oscillation from silence.  Keep the same amplitude/placement;
        // rack::random is deterministic under the standalone harness shim.
        const float input = (frame.input + 1e-6f * (random::uniform() - 0.5f)) * parameters.inputScale;
        feedforwardLowpass = feedforwardSmoothing * feedforwardLowpass + (1.f - feedforwardSmoothing) * input;
        const float feedforward = input - feedforwardLowpass;

        const float cutoff = ripples::kFreqKnobMax * std::exp2(smoothedVOct);
        const float g = std::tan(static_cast<float>(M_PI) * cutoff * sampleTime);
        const float a = g / (1.f + g);
        const float b = 1.f / (1.f + g);

        // Each physical cell is inverting.  The standard TPT state stores
        // z=-v_cell, hence physical output y=-z.  Build y4=alpha*u+beta so
        // the nonlinear feedback can be solved without mutating state.
        float alpha = 1.f;
        float beta = 0.f;
        float outputs[4]{};
        for (int i = 0; i < 4; ++i) {
            alpha = -a * alpha;
            beta = -a * beta - b * stageState[i];
            outputs[i] = beta; // replaced below for the solved input
        }

        const float inputGain = ripples::kFilterInputGain;
        const float feedbackCutoff = std::max(cutoff, ripples::kFreqKnobMin) / 1000.f;
        const float feedbackTilt = std::clamp(std::pow(feedbackCutoff, parameters.feedbackCutoffTilt), 0.05f, 20.f);
        const float otaScale = ripples::kFilterCellR * parameters.feedbackScale * feedbackTilt;
        const float baseInput = input * inputGain;
        float filterInput = baseInput;
        // Abrupt high-cutoff changes can need more than two updates. Stop
        // at float-scale accuracy without advancing the frozen cell states.
        for (int iteration = 0; iteration < 5; ++iteration) {
            const float lp4 = alpha * filterInput + beta;
            const float feedbackVoltage = lp4 * ripples::kFeedbackGain;
            const float ota = OTA(feedforward * ripples::kFeedforwardGain, feedbackVoltage, smoothedResCurrent);
            const float feedback = otaScale * ota;
            const float residual = filterInput - baseInput - feedback;
            const float tolerance = 2e-7f * (1.f + std::abs(baseInput) + std::abs(feedback));
            if (std::abs(residual) <= tolerance) {
                break;
            }
            // OTA(vp, vn, i) differentiates as dOTA/dvi, vi=vp-vn.  Since
            // vn=kFeedbackGain*y4 and y4=alpha*u+beta, the feedback term's
            // contribution to d(u-base-OTA)/du is +dOTA/dvi*kFeedbackGain*
            // alpha*otaScale.
            const float derivative =
                1.f + otaDerivative(feedforward * ripples::kFeedforwardGain, feedbackVoltage, smoothedResCurrent) *
                          ripples::kFeedbackGain * alpha * otaScale;
            filterInput -= residual / std::max(derivative, 0.25f);
        }

        float stageInputAlpha = 1.f;
        float stageInputBeta = 0.f;
        for (int i = 0; i < 4; ++i) {
            const float oldState = stageState[i];
            const float z = a * (stageInputAlpha * filterInput + stageInputBeta) + b * oldState;
            const float y = -z;
            stageState[i] = 2.f * z - oldState;
            stageInputAlpha = -a * stageInputAlpha;
            stageInputBeta = -a * stageInputBeta - b * oldState;
            outputs[i] = y;
        }

        float bp4 = outputs[1] + 2.f * outputs[2] + outputs[3];
        float hp2 = filterInput + 2.f * outputs[0] + outputs[1];
        if (frame.addLowend) {
            hp2 += resonanceKnob * outputs[0];
        }

        const float gainCompensation = frame.gainCompensation ? 1.f / (0.5f + 0.5f * std::exp(-7.f * frame.res_knob)) : 1.f;
        frame.hp2 = hp2 * ripples::kHP2Gain * parameters.hpOutputGain * parameters.outputScale;
        frame.bp4 = bp4 * ripples::kBP4Gain * parameters.bpOutputGain * parameters.outputScale * gainCompensation;
        frame.lp4 = outputs[3] * ripples::kLP4Gain * parameters.outputScale * gainCompensation;

        if (frame.clipOutputs) {
            frame.hp2 = clipOutput(frame.hp2);
            frame.bp4 = clipOutput(frame.bp4);
            frame.lp4 = clipOutput(frame.lp4);
        }

        frame.unused = 0.f;
    }

  private:
    Parameters parameters;
    float sampleTime = 1.f;
    float controlSmoothing = 0.f;
    float feedforwardSmoothing = 0.f;
    float stageState[4]{};
    float smoothedVOct = 0.f;
    float smoothedResCurrent = 0.f;
    float feedforwardLowpass = 0.f;

    static float VtoIConverter(float rfb, float vc, float rc, float vp, float rp) {
        const float vnom = -(vc * rfb / rc + vp * rfb / rp);
        const float vout = std::max(vnom, ripples::kVtoICollectorVSat);
        const float nrc = rp * rfb;
        const float nrp = rc * rfb;
        const float nrfb = rc * rp;
        const float vneg = (vc * nrc + vp * nrp + vout * nrfb) / (nrc + nrp + nrfb);
        return std::max((vneg - vout) / rfb, 0.f);
    }

    // Same bounded Pade tanh used by Ripples.  Keeping this curve, rather
    // than using a hard clip, is important for the characteristic ping decay.
    static float OTA(float vp, float vn, float iAbc) {
        constexpr float vt = 8.617333262145e-5f * (40.f + 273.15f);
        constexpr float zlim = 3.4641016151377544f;
        const float z = std::clamp((vp - vn) / (2.f * vt), -zlim, zlim);
        const float z2 = z * z;
        const float q = 12.f + z2;
        const float denominator = 36.f * z2 + q * q;
        return iAbc * (12.f * z * q / denominator);
    }

    static float otaDerivative(float vp, float vn, float iAbc) {
        constexpr float vt = 8.617333262145e-5f * (40.f + 273.15f);
        constexpr float zlim = 3.4641016151377544f;
        const float rawZ = (vp - vn) / (2.f * vt);
        if (rawZ <= -zlim || rawZ >= zlim) {
            return 0.f;
        }
        const float z = rawZ;
        const float z2 = z * z;
        const float q = 12.f + z2;
        const float n = 12.f * z * q;
        const float dn = 12.f * (12.f + 3.f * z2);
        const float d = 36.f * z2 + q * q;
        const float dd = z * (120.f + 4.f * z2);
        const float dpdz = (dn * d - n * dd) / (d * d);
        return iAbc * dpdz / (2.f * vt);
    }

    static float clipOutput(float value) {
        constexpr float limit = 1.16691853009184f;
        const float x = std::clamp(value * 0.1f, -limit, limit);
        const float x2 = x * x;
        const float x12 = x2 * x2 * x2 * x2 * x2 * x2;
        const float x13 = x12 * x;
        const float x24 = x12 * x12;
        const float x25 = x24 * x;
        const float x36 = x24 * x12;
        const float x37 = x36 * x;
        return 10.f * (x + 1.45833f * x13 + 0.559028f * x25 + 0.0427035f * x37) /
               (1.f + 1.54167f * x12 + 0.642361f * x24 + 0.0579909f * x36);
    }
};

// Four-lane runtime variant.  The fitting knobs above are intentionally not
// part of this path: Atlas uses the physical/default Economy coefficients at
// runtime, while the scalar class remains available to the response harness.
struct AtlasTptEngine4 {
    using float_4 = simd::float_4;

    AtlasTptEngine4() { setSampleRate(1.f); }

    void setSampleRate(float sampleRate) {
        sampleRate = std::max(sampleRate, 1.f);
        sampleTime = 1.f / sampleRate;
        const float controlPole = 6000.f;
        controlSmoothing = std::exp(-2.f * static_cast<float>(M_PI) * controlPole * sampleTime);
        const float feedforwardPole = 1.f / (2.f * static_cast<float>(M_PI) * ripples::kFeedforwardR * ripples::kFeedforwardC);
        feedforwardSmoothing = std::exp(-2.f * static_cast<float>(M_PI) * feedforwardPole * sampleTime);
        reset();
    }

    void reset() {
        stageState[0] = stageState[1] = stageState[2] = stageState[3] = float_4::zero();
        smoothedVOct = float_4::zero();
        smoothedResCurrent = float_4::zero();
        feedforwardLowpass = float_4::zero();
    }

    // Resetting a lane when it leaves/re-enters a polyphonic group prevents
    // stale filter state from becoming audible when channel counts change.
    void resetLane(int lane) {
        for (auto &state : stageState) {
            state[lane] = 0.f;
        }
        smoothedVOct[lane] = 0.f;
        smoothedResCurrent[lane] = 0.f;
        feedforwardLowpass[lane] = 0.f;
    }

    // Copy one voice's complete filter history between different SIMD layouts.
    // Atlas uses this when it changes between four-row mono and per-row poly.
    void copyLaneFrom(const AtlasTptEngine4 &source, int sourceLane, int destinationLane) {
        for (int stage = 0; stage < 4; ++stage) {
            stageState[stage][destinationLane] = source.stageState[stage][sourceLane];
        }
        smoothedVOct[destinationLane] = source.smoothedVOct[sourceLane];
        smoothedResCurrent[destinationLane] = source.smoothedResCurrent[sourceLane];
        feedforwardLowpass[destinationLane] = source.feedforwardLowpass[sourceLane];
    }

    // mode: 0 = LP, 1 = HP, 2 = BP.  Only the selected output is clipped;
    // calculating the three internal taps is still required by the filter
    // topology and costs less than maintaining three separate paths.
    float_4 process(float_4 input, float_4 frequencyCv, float_4 fmCv, float_4 resonanceCv, float freqKnob, float resKnob,
                    bool addLowend, bool gainCompensationEnabled, bool clipOutputs, int mode) {
        return processCore(input, frequencyCv, fmCv, resonanceCv, float_4(freqKnob), float_4(resKnob), addLowend,
                           gainCompensationEnabled, clipOutputs, mode);
    }

    float_4 process(float_4 input, float_4 frequencyCv, float_4 fmCv, float_4 resonanceCv, float_4 freqKnob, float_4 resKnob,
                    bool addLowend, bool gainCompensationEnabled, bool clipOutputs, float_4 mode) {
        return processCore(input, frequencyCv, fmCv, resonanceCv, freqKnob, resKnob, addLowend, gainCompensationEnabled,
                           clipOutputs, mode);
    }

  private:
    template <typename Mode>
    float_4 processCore(float_4 input, float_4 frequencyCv, float_4 fmCv, float_4 resonanceCv, float_4 freqKnob,
                        float_4 resKnob, bool addLowend, bool gainCompensationEnabled, bool clipOutputs, Mode mode) {
        float_4 vOct = (freqKnob - 1.f) * ripples::kFreqKnobVoltage + frequencyCv + fmCv;
        vOct = simd::fmin(vOct, 0.f);
        const float_4 correctedVOct = simd::fmax(vOct, std::log2(ripples::kFreqKnobMin / ripples::kFreqKnobMax));
        smoothedVOct = controlSmoothing * smoothedVOct + (1.f - controlSmoothing) * correctedVOct;

        const float_4 resonanceKnob = simd::clamp(resKnob + 0.9f * resonanceCv, 0.f, 0.9f);
        // Atlas historically folds resonance CV into res_knob and leaves the
        // Ripples frame's separate res_cv terminal at zero. Preserve that
        // routing rather than applying the same CV twice.
        const float_4 resonanceCurrent = VtoIConverter(ripples::kResAmpR, float_4::zero(), ripples::kResInputR,
                                                       resonanceKnob * ripples::kResKnobV, ripples::kResKnobR);
        smoothedResCurrent = controlSmoothing * smoothedResCurrent + (1.f - controlSmoothing) * resonanceCurrent;

        // Keep the reference dither placement, but generate an independent
        // scalar random value for every lane.
        const float_4 dither(random::uniform(), random::uniform(), random::uniform(), random::uniform());
        input = (input + 1e-6f * (dither - 0.5f));
        feedforwardLowpass = feedforwardSmoothing * feedforwardLowpass + (1.f - feedforwardSmoothing) * input;
        const float_4 feedforward = input - feedforwardLowpass;

        const float_4 cutoff = ripples::kFreqKnobMax * simd::exp(smoothedVOct * std::log(2.f));
        const float_4 g = simd::tan(static_cast<float>(M_PI) * cutoff * sampleTime);
        const float_4 a = g / (1.f + g);
        const float_4 b = 1.f / (1.f + g);

        float_4 alpha = 1.f;
        float_4 beta = 0.f;
        for (int i = 0; i < 4; ++i) {
            alpha = -a * alpha;
            beta = -a * beta - b * stageState[i];
        }

        const float inputGain = ripples::kFilterInputGain;
        const float_4 otaScale = ripples::kFilterCellR;
        const float_4 baseInput = input * inputGain;
        float_4 filterInput = baseInput;
        for (int iteration = 0; iteration < 5; ++iteration) {
            const float_4 lp4 = alpha * filterInput + beta;
            const float_4 feedbackVoltage = lp4 * ripples::kFeedbackGain;
            const float_4 ota = OTA(feedforward * ripples::kFeedforwardGain, feedbackVoltage, smoothedResCurrent);
            const float_4 feedback = otaScale * ota;
            const float_4 residual = filterInput - baseInput - feedback;
            const float_4 tolerance = 2e-7f * (1.f + simd::fabs(baseInput) + simd::fabs(feedback));
            const float_4 active = simd::fabs(residual) > tolerance;
            if (simd::movemask(active) == 0) {
                break;
            }
            const float_4 derivative =
                1.f + otaDerivative(feedforward * ripples::kFeedforwardGain, feedbackVoltage, smoothedResCurrent) *
                          ripples::kFeedbackGain * alpha * otaScale;
            // A lane that has converged keeps its result while other lanes
            // finish; its stopping rule matches the scalar engine.
            filterInput -= simd::ifelse(active, residual / simd::fmax(derivative, 0.25f), float_4::zero());
        }

        float_4 stageInputAlpha = 1.f;
        float_4 stageInputBeta = 0.f;
        float_4 outputs[4];
        for (int i = 0; i < 4; ++i) {
            const float_4 oldState = stageState[i];
            const float_4 z = a * (stageInputAlpha * filterInput + stageInputBeta) + b * oldState;
            outputs[i] = -z;
            stageState[i] = 2.f * z - oldState;
            stageInputAlpha = -a * stageInputAlpha;
            stageInputBeta = -a * stageInputBeta - b * oldState;
        }

        float_4 bp4 = outputs[1] + 2.f * outputs[2] + outputs[3];
        float_4 hp2 = filterInput + 2.f * outputs[0] + outputs[1];
        if (addLowend) {
            hp2 += resonanceKnob * outputs[0];
        }

        const float_4 gainCompensation = gainCompensationEnabled ? gainCompensationFor(resonanceKnob) : float_4(1.f);
        float_4 output = selectOutput(mode, hp2, bp4, outputs[3], gainCompensation);
        if (clipOutputs) {
            output = clipOutput(output);
        }
        return -applyHpGain(mode, output);
    }

    static float_4 selectOutput(int mode, float_4 hp2, float_4 bp4, float_4 lp4, float_4 compensation) {
        if (mode == 0) {
            return lp4 * ripples::kLP4Gain * compensation;
        }
        if (mode == 2) {
            return bp4 * ripples::kBP4Gain * compensation;
        }
        return hp2 * ripples::kHP2Gain;
    }

    static float_4 selectOutput(float_4 mode, float_4 hp2, float_4 bp4, float_4 lp4, float_4 compensation) {
        return simd::ifelse(mode == 0.f, lp4 * ripples::kLP4Gain * compensation,
                            simd::ifelse(mode == 2.f, bp4 * ripples::kBP4Gain * compensation, hp2 * ripples::kHP2Gain));
    }

    // Atlas's HP level correction follows clipping in both layouts.
    static float_4 applyHpGain(int mode, float_4 output) { return mode == 1 ? output * 0.5f : output; }

    static float_4 applyHpGain(float_4 mode, float_4 output) { return output * simd::ifelse(mode == 1.f, 0.5f, 1.f); }

    float sampleTime = 1.f;
    float controlSmoothing = 0.f;
    float feedforwardSmoothing = 0.f;
    float_4 stageState[4]{};
    float_4 smoothedVOct = float_4::zero();
    float_4 smoothedResCurrent = float_4::zero();
    float_4 feedforwardLowpass = float_4::zero();

    static float_4 VtoIConverter(float rfb, float_4 vc, float rc, float_4 vp, float rp) {
        const float_4 vnom = -(vc * rfb / rc + vp * rfb / rp);
        const float_4 vout = simd::fmax(vnom, ripples::kVtoICollectorVSat);
        const float nrc = rp * rfb;
        const float nrp = rc * rfb;
        const float nrfb = rc * rp;
        const float_4 vneg = (vc * nrc + vp * nrp + vout * nrfb) / (nrc + nrp + nrfb);
        return simd::fmax((vneg - vout) / rfb, 0.f);
    }

    static float_4 OTA(float_4 vp, float_4 vn, float_4 iAbc) {
        constexpr float vt = 8.617333262145e-5f * (40.f + 273.15f);
        constexpr float zlim = 3.4641016151377544f;
        const float_4 z = simd::clamp((vp - vn) / (2.f * vt), -zlim, zlim);
        const float_4 z2 = z * z;
        const float_4 q = 12.f + z2;
        const float_4 denominator = 36.f * z2 + q * q;
        return iAbc * (12.f * z * q / denominator);
    }

    static float_4 otaDerivative(float_4 vp, float_4 vn, float_4 iAbc) {
        constexpr float vt = 8.617333262145e-5f * (40.f + 273.15f);
        constexpr float zlim = 3.4641016151377544f;
        const float_4 rawZ = (vp - vn) / (2.f * vt);
        const float_4 z = simd::clamp(rawZ, -zlim, zlim);
        const float_4 z2 = z * z;
        const float_4 q = 12.f + z2;
        const float_4 n = 12.f * z * q;
        const float_4 dn = 12.f * (12.f + 3.f * z2);
        const float_4 d = 36.f * z2 + q * q;
        const float_4 dd = z * (120.f + 4.f * z2);
        const float_4 dpdz = (dn * d - n * dd) / (d * d);
        const float_4 derivative = iAbc * dpdz / (2.f * vt);
        const float_4 saturated = (rawZ <= -zlim) | (rawZ >= zlim);
        return simd::ifelse(saturated, float_4::zero(), derivative);
    }

    static float_4 clipOutput(float_4 value) {
        constexpr float limit = 1.16691853009184f;
        const float_4 x = simd::clamp(value * 0.1f, -limit, limit);
        const float_4 x2 = x * x;
        const float_4 x4 = x2 * x2;
        const float_4 x6 = x4 * x2;
        const float_4 x12 = x6 * x6;
        const float_4 x13 = x12 * x;
        const float_4 x24 = x12 * x12;
        const float_4 x25 = x24 * x;
        const float_4 x36 = x24 * x12;
        const float_4 x37 = x36 * x;
        return 10.f * (x + 1.45833f * x13 + 0.559028f * x25 + 0.0427035f * x37) /
               (1.f + 1.54167f * x12 + 0.642361f * x24 + 0.0579909f * x36);
    }

    static float_4 gainCompensationFor(float_4 resKnob) { return 1.f / (0.5f + 0.5f * simd::exp(-7.f * resKnob)); }
};
