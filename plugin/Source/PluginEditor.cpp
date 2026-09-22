#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;

// ------------------------------------------------------------------ finishes, same as the page
static const Theme THEMES[] = {
    { "Graphite", 0xff1f2124, 0xffffb13d, 0xffe4dfd3, 0xffc9c3b6, 0xffd6d0c2, 0xff8f8a7f, 0x2ed6d0c2, 0x8c000000, 0xffece7da, { 52, 30, 18, 62, 36, 20 } },
    { "Arctic",   0xffd9dbdc, 0xff3b8cff, 0xff1d1f22, 0xff2b2e33, 0xff1f2226, 0xff5d6168, 0x4014161a, 0x38000000, 0xff1b1d20, { 26, 26, 28, 20, 20, 22 } },
    { "Olive",    0xff3a3f2c, 0xffff8a2a, 0xffe8e4d4, 0xffd6d1bd, 0xffe0dbc6, 0xff9d9a86, 0x33e0dbc6, 0x80000000, 0xffefeadb, { 58, 34, 20, 64, 38, 22 } },
    { "Oxblood",  0xff4a1d1d, 0xffffd27a, 0xfff1e3d3, 0xffe3cfbd, 0xfff0dcc8, 0xffb39584, 0x33f0dcc8, 0x80000000, 0xfff6e7d6, { 40, 24, 16, 48, 28, 18 } },
    { "Navy",     0xff1b2638, 0xff5fe0c8, 0xffe1e6ee, 0xffc4ccd8, 0xffd4dbe6, 0xff8793a6, 0x2ed2dceb, 0x80000000, 0xffe9eef5, { 48, 30, 20, 56, 34, 22 } },
    { "Sand",     0xffcbbd9f, 0xffe0422b, 0xff2a241b, 0xff3a3226, 0xff2c261d, 0xff6e6350, 0x47281a1a, 0x403c280a, 0xff2a241b, { 62, 38, 22, 66, 40, 24 } },
};
static constexpr int NUM_THEMES = 6;
static constexpr int W = 1360, H = 960;

static Colour dim (Colour c, float k) { return Colour::fromFloatRGBA (c.getFloatRed() * k, c.getFloatGreen() * k, c.getFloatBlue() * k, 1.f); }
static uint32_t xs (uint32_t& s) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
static float rnd (uint32_t& s) { return (float) ((double) xs (s) / 4294967296.0); }

static Image makeWood (int w, int h, uint32_t seed, const int* c) {
    Image im (Image::RGB, w, h, false); Image::BitmapData bd (im, Image::BitmapData::writeOnly);
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        const double u = x + 6 * std::sin (y * .011 + x * .05) + 3 * std::sin (y * .047);
        const double ring = std::sin (u * .9 + std::sin (y * .003) * 4), fine = std::sin (u * 5.3 + y * .02) * .5;
        float k = (float) (.5 + .3 * ring + .12 * fine + (rnd (seed) - .5) * .08); k = jlimit (0.f, 1.f, k);
        bd.setPixelColour (x, y, Colour ((uint8) (c[0] + k * c[3]), (uint8) (c[1] + k * c[4]), (uint8) (c[2] + k * c[5])));
    }
    return im;
}
static Image makeBrushed() {
    Image im (Image::ARGB, 600, 120, true); Image::BitmapData bd (im, Image::BitmapData::writeOnly); uint32_t s = 5;
    for (int y = 0; y < 120; ++y) { float v = rnd (s); for (int x = 0; x < 600; ++x) { v = v * .985f + rnd (s) * .015f; const float k = v + (rnd (s) - .5f) * .1f;
        bd.setPixelColour (x, y, k > .5f ? Colour::fromFloatRGBA (1, 1, 1, (k - .5f) * .09f) : Colour::fromFloatRGBA (0, 0, 0, (.5f - k) * .12f)); } }
    return im;
}

// value text in the same style as the page
static String fmt (int id, float v) {
    const auto& s = vp::specs[id];
    if (s.type == vp::Bool) return v > .5f ? "ON" : "OFF";
    if (s.type == vp::Choice) return String::fromUTF8 (s.names[jlimit (0, s.n - 1, (int) std::lround (v))]);
    const String k (s.id);
    auto hz = [] (float x) { return x >= 1000 ? String (x / 1000, x >= 10000 ? 1 : 2) + " kHz" : String (roundToInt (x)) + " Hz"; };
    auto sec = [] (float x) { return x < 1 ? String (roundToInt (x * 1000)) + " ms" : String (x, 2) + " s"; };
    auto sgn = [] (float x, const char* u) { return (x > 0 ? "+" : "") + String (roundToInt (x)) + u; };
    if (k == "f_cut" || k == "noise_tone" || k == "dl_tone") return hz (v);
    if (k.endsWith ("_a") || k.endsWith ("_d") || k.endsWith ("_r") || k == "v_glide" || k == "fm_dec" || k == "pe_dec") return sec (v);
    if (k.endsWith ("_semi") || k == "pe_amt") return sgn (v, " st");
    if (k.endsWith ("_fine")) return sgn (v, " ct");
    if (k.endsWith ("_det")) return String (roundToInt (v)) + " ct";
    if (k == "f_env") return sgn (v * 100, "%");
    if (k.endsWith ("_rate")) return String (v, v < 1 ? 2 : 1) + " Hz";
    if (k == "rv_size") return String (v, 1) + " s";
    if (k == "bpm") return String (roundToInt (v)) + " bpm";
    if (k.endsWith ("_uni") || k == "v_voices") return String (roundToInt (v));
    return String (roundToInt (v * 100)) + "%";
}

// ------------------------------------------------------------------ controls
struct Ctl : public Component { VireoEditor& ed; int id; Ctl (VireoEditor& e, int i) : ed (e), id (i) {} };

class Knob : public Slider {
public:
    Knob (VireoEditor& e, int i, const String& lab, int size) : ed (e), id (i), label (lab), d (size == 0 ? 34 : size == 1 ? 44 : 62) {
        setSliderStyle (RotaryVerticalDrag); setTextBoxStyle (NoTextBox, true, 0, 0);
        setMouseDragSensitivity (180); setVelocityBasedMode (false);
        att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (e.proc.apvts, vp::specs[i].id, *this);
        setDoubleClickReturnValue (true, (double) vp::specs[i].def);
        bip = vp::specs[i].min < 0 && vp::specs[i].max > 0;
        onValueChange = [this] { ed.touched (id); };
    }
    int w() const { return d + 20; } int h() const { return d + 16 + 16; }
    void mouseEnter (const MouseEvent& e) override { ed.touched (id); Slider::mouseEnter (e); }
    void paint (Graphics& g) override {
        const auto& th = ed.theme(); const Colour led (th.led);
        const float cx = getWidth() * .5f, cy = (d + 16) * .5f, R = (d + 16) * .5f - 1.5f;
        const double t = valueToProportionOfLength (getValue());
        const int N = d > 50 ? 29 : 21, lit = (int) std::lround (t * (N - 1)); const float mid = (N - 1) * .5f;
        for (int i = 0; i < N; ++i) {
            const float a = degreesToRadians (-135.f + 270.f * i / (N - 1)), x = cx + std::sin (a) * R, y = cy - std::cos (a) * R;
            const bool on = bip ? (i >= std::min ((float) lit, mid) && i <= std::max ((float) lit, mid)) : (i <= lit && t > .001);
            const float r = d < 40 ? 1.3f : 1.6f;
            if (on) { g.setColour (led.withAlpha (.25f)); g.fillEllipse (x - r * 2.4f, y - r * 2.4f, r * 4.8f, r * 4.8f); }
            g.setColour (on ? led : dim (led, .22f)); g.fillEllipse (x - r, y - r, r * 2, r * 2);
        }
        const float bx = cx - d * .5f, by = cy - d * .5f;
        // drop shadow and body
        g.setColour (Colours::black.withAlpha (.45f)); g.fillEllipse (bx + 1, by + 5, (float) d, (float) d);
        ColourGradient body (Colour (0xff44464b), cx, by + d * .3f, Colour (0xff151618), cx, by + d, true);
        body.addColour (.55, Colour (0xff26272a)); g.setGradientFill (body); g.fillEllipse (bx, by, (float) d, (float) d);
        // knurled skirt that turns with the knob
        const float ang = degreesToRadians (-135.f + 270.f * (float) t);
        for (int i = 0; i < 36; ++i) { const float a = ang + i * MathConstants<float>::twoPi / 36;
            g.setColour (i % 2 ? Colours::black.withAlpha (.3f) : Colours::white.withAlpha (.07f));
            g.drawLine (cx + std::sin (a) * d * .34f, cy - std::cos (a) * d * .34f, cx + std::sin (a) * d * .49f, cy - std::cos (a) * d * .49f, 1.6f); }
        g.setColour (Colours::white.withAlpha (.12f)); g.drawEllipse (bx + .5f, by + .5f, d - 1.f, d - 1.f, 1.f);
        // pointer
        g.setColour (Colour (0xfff2eee4));
        g.drawLine (cx + std::sin (ang) * d * .46f, cy - std::cos (ang) * d * .46f, cx + std::sin (ang) * d * .26f, cy - std::cos (ang) * d * .26f, d > 50 ? 3.f : 2.4f);
        // brushed aluminium cap, lit from a fixed direction
        const float cr = d * .23f; const int seg = 48;
        for (int i = 0; i < seg; ++i) { const float a0 = i * MathConstants<float>::twoPi / seg, a1 = (i + 1.2f) * MathConstants<float>::twoPi / seg;
            const float k = .52f + .3f * std::cos (2 * (a0 - .4f)) + .06f * std::cos (6 * a0);
            Path p; p.addPieSegment (cx - cr, cy - cr, cr * 2, cr * 2, a0, a1, 0); g.setColour (Colour::fromFloatRGBA (k, k * 1.01f, k * 1.03f, 1)); g.fillPath (p); }
        g.setColour (Colours::black.withAlpha (.4f)); g.drawEllipse (cx - cr, cy - cr, cr * 2, cr * 2, 1.f);
        g.setColour (Colour (th.label)); g.setFont (ed.silk (11.f).withExtraKerningFactor (.14f));
        g.drawText (label.toUpperCase(), Rectangle<float> (0, (float) d + 17, (float) getWidth(), 14), Justification::centred);
    }
private:
    VireoEditor& ed; int id; String label; int d; bool bip = false;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> att;
};

class Fader : public Slider {
public:
    Fader (VireoEditor& e, int i, const String& lab) : ed (e), id (i), label (lab) {
        setSliderStyle (LinearVertical); setTextBoxStyle (NoTextBox, true, 0, 0); setSliderSnapsToMousePosition (false);
        att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (e.proc.apvts, vp::specs[i].id, *this);
        setDoubleClickReturnValue (true, (double) vp::specs[i].def);
        onValueChange = [this] { ed.touched (id); };
    }
    void mouseEnter (const MouseEvent& e) override { ed.touched (id); Slider::mouseEnter (e); }
    void paint (Graphics& g) override {
        const auto& th = ed.theme(); const float top = 4, bot = 108, t = (float) valueToProportionOfLength (getValue());
        g.setColour (Colour (th.rule)); for (float y = top; y <= bot; y += 10) g.fillRect (0.f, y, 8.f, 1.f);
        g.setColour (Colour (0xff050606)); g.fillRoundedRectangle (14, top, 4, bot - top, 2);
        const float y = top + (1 - t) * (bot - top);
        g.setColour (Colours::black.withAlpha (.5f)); g.fillRoundedRectangle (5, y - 5, 24, 15, 3);
        ColourGradient cg (Colour (0xff5d6066), 0, y - 7, Colour (0xff45484d), 0, y + 8, false); cg.addColour (.45, Colour (0xff34363a)); cg.addColour (.55, Colour (0xff2a2c2f));
        g.setGradientFill (cg); g.fillRoundedRectangle (4, y - 7, 24, 15, 3);
        g.setColour (Colour (0xffece7da)); g.fillRoundedRectangle (7, y - 1, 18, 2, 1);
        g.setColour (Colour (th.label)); g.setFont (ed.silk (11.f).withExtraKerningFactor (.14f));
        g.drawText (label, Rectangle<float> (0, 110, (float) getWidth(), 14), Justification::centred);
    }
private:
    VireoEditor& ed; int id; String label; std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> att;
};

class Sel : public Ctl {
public:
    Sel (VireoEditor& e, int i, const String& lab) : Ctl (e, i), label (lab),
        att (*e.proc.apvts.getParameter (vp::specs[i].id), [this] (float v) { value = (int) std::lround (v); repaint(); }) { att.sendInitialUpdate(); }
    void paint (Graphics& g) override {
        const auto& th = ed.theme();
        g.setColour (Colour (0xff9a9489)); g.setFont (ed.silk (12.f, true));
        g.drawText (CharPointer_UTF8 ("\xe2\x97\x80"), Rectangle<float> (0, 0, 16, 24), Justification::centred);
        g.drawText (CharPointer_UTF8 ("\xe2\x96\xb6"), Rectangle<float> ((float) getWidth() - 16, 0, 16, 24), Justification::centred);
        auto box = Rectangle<float> (18, 0, (float) getWidth() - 36, 24);
        g.setColour (Colour (0xff0a0d0b)); g.fillRoundedRectangle (box, 3); g.setColour (Colours::black); g.drawRoundedRectangle (box, 3, 1);
        g.setColour (Colour (th.led)); g.setFont (ed.lcd (14.f)); g.drawText (fmt (id, (float) value), box, Justification::centred);
        g.setColour (Colour (th.label)); g.setFont (ed.silk (11.f).withExtraKerningFactor (.14f)); g.drawText (label.toUpperCase(), Rectangle<float> (0, 27, (float) getWidth(), 14), Justification::centred);
    }
    void mouseDown (const MouseEvent& e) override { step (e.x < 18 ? -1 : 1); }
    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w) override { step (w.deltaY < 0 ? 1 : -1); }
    void step (int dir) { const int n = vp::specs[id].n; att.setValueAsCompleteGesture ((float) ((value + dir + n) % n)); ed.touched (id); }
private:
    String label; int value = 0; ParameterAttachment att;
};

class Led : public Ctl {
public:
    Led (VireoEditor& e, int i) : Ctl (e, i), att (*e.proc.apvts.getParameter (vp::specs[i].id), [this] (float v) { on = v > .5f; repaint(); }) { att.sendInitialUpdate(); }
    void paint (Graphics& g) override;
    void mouseDown (const MouseEvent&) override { att.setValueAsCompleteGesture (on ? 0.f : 1.f); }
    bool on = false;
private:
    ParameterAttachment att;
};

static void hwButton (Graphics& g, Rectangle<float> r, const VireoEditor& ed, const String& text, int ledState, bool down = false) {
    const auto& th = ed.theme();
    if (! down) { g.setColour (Colour (0xff0a0b0c)); g.fillRoundedRectangle (r.translated (0, 2), 4); }
    ColourGradient cg (Colour (0xff3a3c40), 0, r.getY(), Colour (0xff26282b), 0, r.getBottom(), false); g.setGradientFill (cg); g.fillRoundedRectangle (r.translated (0, down ? 1.f : 0.f), 4);
    g.setColour (Colours::white.withAlpha (.1f)); g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 3, r.getRight() - 3);
    float tx = r.getX() + 8;
    if (ledState >= 0) { const Colour l (th.led); if (ledState) { g.setColour (l.withAlpha (.35f)); g.fillEllipse (tx - 3, r.getCentreY() - 6, 12, 12); }
        g.setColour (ledState ? l : dim (l, .22f)); g.fillEllipse (tx, r.getCentreY() - 3, 6, 6); tx += 10; }
    g.setColour (Colour (0xffd8d3c7)); g.setFont (ed.silk (11.f).withExtraKerningFactor (.14f));
    g.drawText (text.toUpperCase(), Rectangle<float> (tx, r.getY(), r.getRight() - tx - 6, r.getHeight()), ledState >= 0 ? Justification::centredLeft : Justification::centred);
}
void Led::paint (Graphics& g) { hwButton (g, getLocalBounds().toFloat().withTrimmedBottom (2), ed, "On", on ? 1 : 0); }

struct HwButton : public Component {
    HwButton (VireoEditor& e, String t, std::function<void()> f) : ed (e), text (std::move (t)), fn (std::move (f)) {}
    void paint (Graphics& g) override { hwButton (g, getLocalBounds().toFloat().withTrimmedBottom (2), ed, text, -1, down); }
    void mouseDown (const MouseEvent&) override { down = true; repaint(); }
    void mouseUp (const MouseEvent& e) override { down = false; repaint(); if (getLocalBounds().contains (e.getPosition())) fn(); }
    VireoEditor& ed; String text; std::function<void()> fn; bool down = false;
};

// ------------------------------------------------------------------ layout: rows and columns of controls
struct Node {
    Component* c = nullptr; int w = 0, h = 0; bool row = true; std::vector<Node> kids;
    static Node leaf (Component* c, int w, int h) { Node n; n.c = c; n.w = w; n.h = h; return n; }
    static Node box (bool row, std::vector<Node> k) { Node n; n.row = row; n.kids = std::move (k); return n; }
    Point<int> size() const {
        if (c) return { w, h };
        int a = 0, b = 0, i = 0;
        for (auto& k : kids) { auto s = k.size(); if (row) { a += s.x + (i ? 4 : 0); b = std::max (b, s.y); } else { b += s.y + (i ? 6 : 0); a = std::max (a, s.x); } ++i; }
        return { a, b };
    }
    void place (int x, int y) const {
        if (c) { c->setBounds (x, y, w, h); return; }
        for (auto& k : kids) { auto s = k.size(); k.place (x, y); if (row) x += s.x + 4; else y += s.y + 6; }
    }
};

class Section : public Component {
public:
    Section (VireoEditor& e, String t) : ed (e), title (std::move (t)) {}
    void paint (Graphics& g) override {
        const auto& th = ed.theme(); auto r = getLocalBounds().toFloat();
        g.setColour (Colours::white.withAlpha (.025f)); g.fillRoundedRectangle (r, 7);
        g.setColour (Colour (th.well)); g.drawRoundedRectangle (r.reduced (.5f), 7, 1);
        g.setColour (Colour (th.title)); g.setFont (ed.silk (12.5f, true).withExtraKerningFactor (.24f));
        const float tw = GlyphArrangement::getStringWidth (g.getCurrentFont(), title.toUpperCase()) + 12;
        g.drawText (title.toUpperCase(), Rectangle<float> (12, 5, r.getWidth(), 16), Justification::centredLeft);
        g.setColour (Colour (th.rule)); g.fillRect (12 + tw, 13.f, r.getWidth() - 24 - tw - (led ? 52 : 0), 1.f);
    }
    void resized() override { if (led) led->setBounds (getWidth() - 58, 5, 46, 20); layout.place (12, 30); }
    VireoEditor& ed; String title; Node layout; Led* led = nullptr;
};

// ------------------------------------------------------------------ the screen
class Screen : public Component, private Timer {
public:
    explicit Screen (VireoEditor& e) : ed (e) { tables.build(); startTimerHz (30); }
    void timerCallback() override { repaint(); }
    float pv (int id) const { return ed.proc.apvts.getRawParameterValue (vp::specs[id].id)->load(); }
    void paint (Graphics& g) override {
        const auto& th = ed.theme(); const Colour A (th.led), D = A.withAlpha (.28f), G = A.withAlpha (.08f);
        auto r = getLocalBounds().toFloat();
        g.setColour (Colour (0xff070908)); g.fillRoundedRectangle (r, 7);
        auto in = r.reduced (8); g.setColour (Colour (0xff0a0d0b)); g.fillRoundedRectangle (in, 4);
        const float Wd = in.getWidth(), Hh = in.getHeight(), X0 = in.getX(), Y0 = in.getY();
        g.setColour (A); g.setFont (ed.lcd (Hh * .075f));
        g.drawText (String (vp::presets[ed.proc.program.load()].name).toUpperCase(), Rectangle<float> (X0 + 12, Y0 + 4, Wd * .6f, Hh * .09f), Justification::centredLeft);
        g.setColour (D); g.setFont (ed.lcd (Hh * .05f));
        const int mode = (int) pv (vp::v_mode); const bool arp = pv (vp::arp_on) > .5f;
        g.drawText (String (vp::nm_v_mode[mode]) + " / " + String (roundToInt (pv (vp::bpm))) + " BPM" + (arp ? String (" / ARP ") + vp::nm_arp_div[(int) pv (vp::arp_div)] : String()),
                    Rectangle<float> (X0 + Wd * .4f, Y0 + 6, Wd * .6f - 12, Hh * .06f), Justification::centredRight);
        const float pw = (Wd - 40) / 3, ph = Hh * .44f, py = Y0 + Hh * .12f;
        auto box = [&] (int i, const String& t) { const float bx = X0 + 10 + i * (pw + 10); g.setColour (G); g.drawRect (bx, py, pw, ph, 1.f); g.setColour (D); g.setFont (ed.lcd (Hh * .05f)); g.drawText (t, Rectangle<float> (bx + 6, py + 3, pw, 14), Justification::centredLeft); return bx; };
        // oscillators
        float bx = box (0, "OSC");
        for (int o = 1; o >= 0; --o) {
            const int on = o ? vp::o2_on : vp::o1_on; if (pv (on) < .5f) continue;
            const int w = (int) pv (on + 1); const float pwid = pv (on + 9), lvl = pv (on + 5);
            Path p; for (int i = 0; i <= 160; ++i) { const float u = i / 160.f; const double ph0 = std::fmod (u * 2.0, 1.0);
                float v = vireo::oscSample (w, ph0, 1e-6, pwid, tables, 40.f, 48000.f); if (w == vireo::SAW) v = (float) (1 - 2 * ph0);
                const float x = bx + 8 + u * (pw - 16), y = py + ph * .5f - jlimit (-1.2f, 1.2f, v) * lvl * ph * .36f; i ? p.lineTo (x, y) : p.startNewSubPath (x, y); }
            g.setColour (o ? D : A); g.strokePath (p, PathStrokeType (2.f));
        }
        // filter response, from the state variable filter's analytic magnitude
        const int ty = (int) pv (vp::f_type); const float res = pv (vp::f_res), fc = pv (vp::f_cut);
        bx = box (1, String ("FILTER ") + vp::nm_f_type[ty]);
        float q = ty <= 2 ? std::pow (10.f, (res * res * 26 - 1) / 20) : .6f + res * 18; q = std::max (.5f, q);
        Path fp;
        for (int i = 0; i < 120; ++i) { const float f = 20 * std::pow (1000.f, i / 119.f), w0 = f / fc;
            auto mag = [&] (float Q, int m) { const float den = std::sqrt ((1 - w0 * w0) * (1 - w0 * w0) + (w0 / Q) * (w0 / Q));
                return (m == 0 ? 1.f : m == 1 ? w0 * w0 : m == 2 ? w0 / Q : std::abs (1 - w0 * w0)) / den; };
            float m = ty <= 1 ? mag (q, 0) : ty == 2 ? mag (q, 1) : ty == 3 ? mag (q, 2) : mag (q, 3); if (ty == 0) m *= mag (.7071f, 0);
            const float db = 20 * std::log10 (std::max (1e-5f, m)), x = bx + 6 + i / 119.f * (pw - 12), y = jlimit (py + 14, py + ph - 4, py + ph * .42f - db / 48 * ph * .5f);
            i ? fp.lineTo (x, y) : fp.startNewSubPath (x, y); }
        g.setColour (A); g.strokePath (fp, PathStrokeType (2.f));
        // envelopes
        bx = box (2, "ENV");
        for (int e = 0; e < 2; ++e) {
            const int b = e ? vp::ae_a : vp::fe_a; const float a = pv (b), dd = pv (b + 1), s = pv (b + 2), rr = pv (b + 3);
            const float tot = std::max (1.2f, a + dd * 2 + .4f + rr * 1.5f);
            auto X = [&] (float t) { return bx + 8 + t / tot * (pw - 16); }; auto Y = [&] (float v) { return py + ph - 6 - v * (ph - 26); };
            Path p; p.startNewSubPath (X (0), Y (0)); p.lineTo (X (a), Y (1));
            for (int i = 1; i <= 20; ++i) { const float t = dd * 2 * i / 20; p.lineTo (X (a + t), Y (s + (1 - s) * std::exp (-t * 3 / dd))); }
            p.lineTo (X (a + dd * 2 + .4f), Y (s));
            for (int i = 1; i <= 20; ++i) { const float t = rr * 1.5f * i / 20; p.lineTo (X (a + dd * 2 + .4f + t), Y (s * std::exp (-t * 3 / rr))); }
            g.setColour (e ? A : D); g.strokePath (p, PathStrokeType (2.f));
        }
        // scope, triggered on a rising zero crossing
        const float top = Y0 + Hh * .6f, hh = Hh * .36f;
        g.setColour (G); g.drawRect (X0 + 10, top, Wd - 20, hh, 1.f);
        const int w = ed.proc.scopeW.load(), S = VireoProcessor::SCOPE; int start = (w - 1800 + S) & (S - 1);
        for (int i = 0; i < 800; ++i) { const int a = (start + i) & (S - 1), b2 = (a + 1) & (S - 1); if (ed.proc.scope[a] < 0 && ed.proc.scope[b2] >= 0) { start = b2; break; } }
        Path sp; for (int i = 0; i < 900; ++i) { const float v = ed.proc.scope[(start + i) & (S - 1)], x = X0 + 12 + i / 899.f * (Wd - 24), y = top + hh * .5f - jlimit (-1.f, 1.f, v) * hh * .9f; i ? sp.lineTo (x, y) : sp.startNewSubPath (x, y); }
        g.setColour (A); g.strokePath (sp, PathStrokeType (1.4f));
        const int nv = ed.proc.activeVoices();
        g.setColour (D); g.setFont (ed.lcd (Hh * .045f)); g.drawText (String (nv) + (nv == 1 ? " VOICE" : " VOICES"), Rectangle<float> (X0 + 16, top + hh - 18, 200, 14), Justification::centredLeft);
        if (ed.statusId >= 0 && Time::getMillisecondCounter() - ed.statusT < 2200) {
            const String t = String (vp::specs[ed.statusId].name).toUpperCase() + "  " + fmt (ed.statusId, pv (ed.statusId));
            g.setFont (ed.lcd (Hh * .06f)); const float tw = GlyphArrangement::getStringWidth (g.getCurrentFont(), t) + 20;
            g.setColour (Colour (0xff0a0d0b)); g.fillRect (X0 + Wd - tw - 14, top + 6, tw, Hh * .09f);
            g.setColour (A); g.drawText (t, Rectangle<float> (X0 + Wd - tw - 4, top + 6, tw, Hh * .09f), Justification::centredLeft);
        }
    }
    VireoEditor& ed; vireo::Tables tables;
};

// ------------------------------------------------------------------ top bar pieces
struct PresetLcd : public Component {
    explicit PresetLcd (VireoEditor& e) : ed (e) {}
    void paint (Graphics& g) override {
        auto r = getLocalBounds().toFloat(); g.setColour (Colour (0xff0a0d0b)); g.fillRoundedRectangle (r, 5); g.setColour (Colours::black); g.drawRoundedRectangle (r, 5, 1);
        const int i = ed.proc.program.load(); const Colour A (ed.theme().led);
        g.setColour (A.withAlpha (.7f)); g.setFont (ed.lcd (12)); g.drawText (String (vp::presets[i].cat) + " / " + String (i + 1).paddedLeft ('0', 3), r.reduced (12, 6).withHeight (14), Justification::centredLeft);
        g.setColour (A); g.setFont (ed.lcd (22)); g.drawText (vp::presets[i].name, r.reduced (12, 6).withTrimmedTop (16), Justification::centredLeft);
    }
    void mouseDown (const MouseEvent&) override {
        PopupMenu m; const char* cats[] = { "BASS", "LEAD", "PAD", "KEYS", "PLUCK", "ARP", "FX" };
        for (auto c : cats) { PopupMenu sub; for (int i = 0; i < vp::NUM_PRESETS; ++i) if (String (vp::presets[i].cat) == c) sub.addItem (i + 1, vp::presets[i].name, true, i == ed.proc.program.load()); m.addSubMenu (c, sub); }
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [this] (int r) { if (r > 0) { ed.proc.setCurrentProgram (r - 1); ed.refreshPresetName(); } });
    }
    VireoEditor& ed;
};

struct Swatches : public Component {
    explicit Swatches (VireoEditor& e) : ed (e) {}
    void paint (Graphics& g) override {
        for (int i = 0; i < NUM_THEMES; ++i) { const float x = 2 + i * 22.f; Rectangle<float> r (x, 2, 16, 16);
            if (i == ed.proc.theme) { g.setColour (Colour (ed.theme().led)); g.drawEllipse (r.expanded (2), 2); }
            Path half; half.addPieSegment (r, 0, MathConstants<float>::pi * 2, 0);
            g.setColour (Colour (THEMES[i].panel)); g.fillEllipse (r);
            Path t; t.addPieSegment (r, MathConstants<float>::pi * .25f, MathConstants<float>::pi * 1.25f, 0); g.setColour (Colour (THEMES[i].led)); g.fillPath (t);
            g.setColour (Colours::white.withAlpha (.25f)); g.drawEllipse (r, 1); }
    }
    void mouseDown (const MouseEvent& e) override { const int i = (e.x - 2) / 22; if (i >= 0 && i < NUM_THEMES) ed.setTheme (i); }
    VireoEditor& ed;
};

struct Meter : public Component, private Timer {
    explicit Meter (VireoEditor& e) : ed (e) { startTimerHz (30); }
    void timerCallback() override { repaint(); }
    void paint (Graphics& g) override {
        g.setColour (Colour (0xff0b0c0d)); g.fillRoundedRectangle (getLocalBounds().toFloat(), 4);
        const float pk[2] = { ed.proc.peakL.load(), ed.proc.peakR.load() };
        for (int c = 0; c < 2; ++c) { const float db = 20 * std::log10 (pk[c] + 1e-6f); const int lit = roundToInt ((db + 48) / 48 * 12);
            for (int j = 0; j < 12; ++j) { const float y = getHeight() - 5 - (j + 1) * ((getHeight() - 8) / 12.f); Rectangle<float> r (5 + c * 10.f, y, 7, (getHeight() - 8) / 12.f - 2);
                g.setColour (j < lit ? Colour (j >= 11 ? 0xffff5a36 : j >= 9 ? 0xffe6c143 : 0xff8fd35a) : Colour (0xff1b1d1b)); g.fillRect (r); } }
    }
    VireoEditor& ed;
};

// ------------------------------------------------------------------ wheels and keys
struct Wheel : public Component {
    Wheel (VireoEditor& e, bool spring, std::atomic<float>& t) : ed (e), springy (spring), target (t) {}
    void paint (Graphics& g) override {
        auto r = getLocalBounds().toFloat().withTrimmedBottom (18);
        g.setColour (Colour (0xff050505)); g.fillRoundedRectangle (r, 5);
        auto w = r.reduced (3); const float v = target.load();
        for (float y = w.getY(); y < w.getBottom(); y += 6) { const float yy = y + std::fmod (v * 40 + 600, 6.f); if (yy + 3 > w.getBottom()) break; g.setColour (Colour (0xff2c2e31)); g.fillRect (w.getX(), yy, w.getWidth(), 3.f); g.setColour (Colour (0xff1a1b1d)); g.fillRect (w.getX(), yy + 3, w.getWidth(), 3.f); }
        ColourGradient sh (Colours::black.withAlpha (.85f), 0, w.getY(), Colours::transparentBlack, 0, w.getY() + 18, false); g.setGradientFill (sh); g.fillRect (w.withHeight (18));
        ColourGradient sb (Colours::transparentBlack, 0, w.getBottom() - 18, Colours::black.withAlpha (.85f), 0, w.getBottom(), false); g.setGradientFill (sb); g.fillRect (w.withTop (w.getBottom() - 18));
        const float y = springy ? w.getY() + (.5f - v / 2) * w.getHeight() : w.getY() + (1 - v) * w.getHeight();
        g.setColour (Colour (0xffece7da)); g.fillRoundedRectangle (w.getX(), y - 2, w.getWidth(), 4, 1);
        g.setColour (Colour (0xffc9c3b6)); g.setFont (ed.silk (11).withExtraKerningFactor (.14f)); g.drawText (springy ? "PITCH" : "MOD", getLocalBounds().toFloat().withTop ((float) getHeight() - 15), Justification::centred);
    }
    void mouseDown (const MouseEvent& e) override { sy = e.y; sv = target.load(); }
    void mouseDrag (const MouseEvent& e) override { float nv = sv + (sy - e.y) / (float) (getHeight() - 18) * (springy ? 2 : 1); target.store (springy ? jlimit (-1.f, 1.f, nv) : jlimit (0.f, 1.f, nv)); repaint(); }
    void mouseUp (const MouseEvent&) override { if (springy) { target.store (0); repaint(); } }
    VireoEditor& ed; bool springy; std::atomic<float>& target; int sy = 0; float sv = 0;
};

class Keys : public MidiKeyboardComponent {
public:
    Keys (VireoEditor& e) : MidiKeyboardComponent (e.proc.keyboard, horizontalKeyboard), ed (e) {
        setAvailableRange (36, 84); setOctaveForMiddleC (4); setScrollButtonsVisible (false); setKeyPressBaseOctave (4);
        setColour (keySeparatorLineColourId, Colour (0xffbdb6a7)); setColour (shadowColourId, Colours::transparentBlack);
        setWantsKeyboardFocus (true); setVelocity (.8f, true);
    }
    bool keyPressed (const KeyPress& k) override {
        const juce_wchar c = CharacterFunctions::toLowerCase (k.getTextCharacter());
        if (c == 'z') { setKeyPressBaseOctave (jmax (2, base - 1)); base = jmax (2, base - 1); repaint(); return true; }
        if (c == 'x') { setKeyPressBaseOctave (jmin (7, base + 1)); base = jmin (7, base + 1); repaint(); return true; }
        return MidiKeyboardComponent::keyPressed (k);
    }
    void drawWhiteNote (int note, Graphics& g, Rectangle<float> a, bool down, bool, Colour, Colour) override {
        ColourGradient cg (Colour (down ? 0xffdcd7cc : 0xffe9e5dc), 0, a.getY(), Colour (down ? 0xffdcd6ca : 0xffe2ddd2), 0, a.getBottom(), false); cg.addColour (.7, Colour (down ? 0xffebe7de : 0xfff6f3ec));
        g.setGradientFill (cg); g.fillRoundedRectangle (a.reduced (.5f, 0).withTrimmedTop (-4), 4);
        g.setColour (Colour (0xffd3cdbf)); g.fillRect (a.getX() + 1, a.getBottom() - (down ? 2.f : 6.f), a.getWidth() - 2, down ? 2.f : 6.f);
        g.setColour (Colour (0xffbdb6a7)); g.fillRect (a.getRight() - 1, a.getY(), 1.f, a.getHeight());
        if (down) { ColourGradient s (Colours::black.withAlpha (.22f), 0, a.getY(), Colours::transparentBlack, 0, a.getY() + 10, false); g.setGradientFill (s); g.fillRect (a.withHeight (10)); }
        const String lab = keyLetter (note);
        g.setColour (Colour (0xffa59d8e)); g.setFont (ed.silk (10));
        if (note % 12 == 0) g.drawText ("C" + String (note / 12 - 1), a.withTrimmedBottom (10).withTop (a.getBottom() - 26), Justification::centred);
        if (lab.isNotEmpty()) { g.setColour (Colour (0xff8f8676)); g.drawText (lab, a.withTop (a.getBottom() - 44).withHeight (14), Justification::centred); }
    }
    void drawBlackNote (int note, Graphics& g, Rectangle<float> a, bool down, bool, Colour) override {
        g.setColour (Colours::black.withAlpha (.6f)); g.fillRoundedRectangle (a.translated (1, 3), 3);
        ColourGradient cg (Colour (0xff141415), a.getX(), 0, Colour (0xff1b1c1e), a.getRight(), 0, false); cg.addColour (.45, Colour (0xff2c2d30));
        g.setGradientFill (cg); g.fillRoundedRectangle (a, 3);
        g.setColour (Colour (0xff0c0c0d)); g.fillRect (a.getX(), a.getBottom() - (down ? 3.f : 8.f), a.getWidth(), down ? 3.f : 8.f);
        g.setColour (Colours::white.withAlpha (.12f)); g.fillRect (a.getX() + 1, a.getBottom() - (down ? 4.f : 9.f), a.getWidth() - 2, 1.f);
        const String lab = keyLetter (note); if (lab.isNotEmpty()) { g.setColour (Colour (0xff77716a)); g.setFont (ed.silk (10)); g.drawText (lab, a.withTop (a.getBottom() - 24).withHeight (12), Justification::centred); }
    }
    String keyLetter (int note) const { static const char* m = "awsedftgyhujkolp;"; const int i = note - base * 12; return i >= 0 && i < 17 ? String::charToString (CharacterFunctions::toUpperCase ((juce_wchar) m[i])) : String(); }
    VireoEditor& ed; int base = 4;
};

static Font silkOf (VireoEditor& e, float h) { return e.silk (h).withExtraKerningFactor (.2f); }

// ------------------------------------------------------------------ the whole face
class Face : public Component {
public:
    explicit Face (VireoEditor& e) : ed (e) {
        auto K = [&] (int id, const char* l, int size = 1) { auto k = std::make_unique<Knob> (e, id, l, size); auto n = Node::leaf (k.get(), k->w(), k->h()); addAndMakeVisible (*k); owned.push_back (std::move (k)); return n; };
        auto Fd = [&] (int id, const char* l) { auto f = std::make_unique<Fader> (e, id, l); auto n = Node::leaf (f.get(), 32, 124); owned.push_back (std::move (f)); return n; };
        auto Sl = [&] (int id, const char* l) { auto s = std::make_unique<Sel> (e, id, l); auto n = Node::leaf (s.get(), 80, 44); owned.push_back (std::move (s)); return n; };
        auto row = [] (std::vector<Node> k) { return Node::box (true, std::move (k)); };
        auto col = [] (std::vector<Node> k) { return Node::box (false, std::move (k)); };
        auto sec = [&] (const char* t, int w, Node lay, int ledId = -1) { auto s = std::make_unique<Section> (e, t); s->layout = std::move (lay); s->setSize (w, 0);
            if (ledId >= 0) { auto l = std::make_unique<Led> (e, ledId); s->led = l.get(); s->addAndMakeVisible (*l); owned.push_back (std::move (l)); }
            addAndMakeVisible (*s); auto* raw = s.get(); sections.push_back (std::move (s)); return raw; };
        using namespace vp;
        for (int o = 0; o < 2; ++o) { const int b = o ? o2_on : o1_on;
            r1.push_back (sec (o ? "Oscillator 2" : "Oscillator 1", 324, row ({ col ({ row ({ Sl (b + 1, "Wave"), Sl (b + 2, "Octave") }), row ({ K (b + 3, "Semi", 0), K (b + 4, "Fine", 0), K (b + 9, "Width", 0) }) }),
                                                                          col ({ row ({ K (b + 5, "Level"), K (b + 6, "Unison") }), row ({ K (b + 7, "Detune", 0), K (b + 8, "Spread", 0) }) }) }), b)); }
        r1.push_back (sec ("Sub / Noise / FM", 250, col ({ row ({ K (sub_lvl, "Sub", 0), Sl (sub_oct, "Sub oct"), Sl (sub_wave, "Sub wave") }),
                                                            row ({ K (noise_lvl, "Noise", 0), K (noise_tone, "Tone", 0), K (fm_amt, "FM", 0), K (fm_dec, "FM Dec", 0) }),
                                                            row ({ Sl (fm_ratio, "FM ratio"), K (pe_amt, "P.Env", 0), K (pe_dec, "P.Dec", 0) }) })));
        r2.push_back (sec ("Filter", 390, row ({ col ({ Sl (f_type, "Mode"), K (f_res, "Reso", 0) }), K (f_cut, "Cutoff", 2), row ({ K (f_env, "Env", 0), K (f_key, "Key", 0), K (f_vel, "Vel", 0) }) })));
        r2.push_back (sec ("Filter envelope", 176, row ({ Fd (fe_a, "A"), Fd (fe_d, "D"), Fd (fe_s, "S"), Fd (fe_r, "R") })));
        r2.push_back (sec ("Amp envelope", 236, row ({ Fd (ae_a, "A"), Fd (ae_d, "D"), Fd (ae_s, "S"), Fd (ae_r, "R"), K (ae_vel, "Vel", 0) })));
        r2.push_back (sec ("LFO 1", 209, row ({ col ({ Sl (l1_shape, "Shape"), Sl (l1_dest, "Dest") }), col ({ K (l1_rate, "Rate"), K (l1_depth, "Depth") }) })));
        r2.push_back (sec ("LFO 2", 209, row ({ col ({ Sl (l2_shape, "Shape"), Sl (l2_dest, "Dest") }), col ({ K (l2_rate, "Rate"), K (l2_depth, "Depth") }) })));
        r3.push_back (sec ("Voice", 330, col ({ row ({ Sl (v_mode, "Mode"), Sl (v_bend, "Bend"), Sl (mw_dest, "Mod wheel") }), row ({ K (v_glide, "Glide", 0), K (v_voices, "Voices", 0), K (bpm, "Tempo", 0) }) })));
        r3.push_back (sec ("Arpeggiator", 250, col ({ row ({ Sl (arp_mode, "Mode"), Sl (arp_div, "Rate") }), row ({ Sl (arp_oct, "Octaves"), K (arp_gate, "Gate", 0) }) }), arp_on));
        r3.push_back (sec ("Drive", 90, K (fx_drive, "Amount")));
        r3.push_back (sec ("Chorus", 180, col ({ row ({ K (ch_rate, "Rate", 0), K (ch_depth, "Depth", 0) }), K (ch_mix, "Mix") })));
        r3.push_back (sec ("Delay", 180, col ({ row ({ Sl (dl_div, "Time"), K (dl_fb, "Fdbk", 0) }), row ({ K (dl_tone, "Tone", 0), K (dl_mix, "Mix") }) })));
        r3.push_back (sec ("Reverb", 180, col ({ row ({ K (rv_size, "Size", 0), K (rv_damp, "Damp", 0) }), K (rv_mix, "Mix") })));
        for (auto& s : sections) addChildren (*s, s->layout);
        screen = std::make_unique<Screen> (e); addAndMakeVisible (*screen);
        lcd = std::make_unique<PresetLcd> (e); addAndMakeVisible (*lcd);
        prev = std::make_unique<HwButton> (e, CharPointer_UTF8 ("\xe2\x97\x80"), [this] { step (-1); }); addAndMakeVisible (*prev);
        next = std::make_unique<HwButton> (e, CharPointer_UTF8 ("\xe2\x96\xb6"), [this] { step (1); }); addAndMakeVisible (*next);
        swatches = std::make_unique<Swatches> (e); addAndMakeVisible (*swatches);
        vol = std::make_unique<Knob> (e, vp::vol, "Volume", 1); addAndMakeVisible (*vol);
        meter = std::make_unique<Meter> (e); addAndMakeVisible (*meter);
        bendW = std::make_unique<Wheel> (e, true, e.proc.uiBend); addAndMakeVisible (*bendW);
        modW = std::make_unique<Wheel> (e, false, e.proc.uiMod); addAndMakeVisible (*modW);
        keys = std::make_unique<Keys> (e); addAndMakeVisible (*keys);
    }
    void addChildren (Section& s, Node& n) { if (n.c) { s.addAndMakeVisible (*n.c); return; } for (auto& k : n.kids) addChildren (s, k); }
    void step (int d) { ed.proc.setCurrentProgram ((ed.proc.program.load() + d + vp::NUM_PRESETS) % vp::NUM_PRESETS); ed.refreshPresetName(); }
    void resized() override {
        const int fx = 34 + 16, fw = W - 68 - 32;
        lcd->setBounds (560, 24, 380, 52); prev->setBounds (946, 24, 32, 25); next->setBounds (946, 51, 32, 25);
        swatches->setBounds (310, 38, 140, 22); vol->setBounds (1190, 14, vol->w(), vol->h()); meter->setBounds (1262, 24, 26, 52);
        auto lay = [&] (std::vector<Section*>& row, int y, int h, Component* tail) {
            int x = fx; for (auto* s : row) { s->setBounds (x, y, s->getWidth(), h); x += s->getWidth() + 10; }
            if (tail) tail->setBounds (x, y, fx + fw - x, h);
        };
        lay (r1, 92, 250, screen.get()); lay (r2, 352, 196, nullptr); lay (r3, 558, 186, nullptr);
        bendW->setBounds (fx + 10, 760, 30, 170); modW->setBounds (fx + 50, 760, 30, 170);
        keys->setBounds (fx + 100, 756, fw - 100, 170); keys->setKeyWidth ((float) (fw - 100) / 29.f);
    }
    void paint (Graphics& g) override {
        const auto& th = ed.theme();
        g.fillAll (Colour (0xff141414));
        auto face = Rectangle<float> (34, 0, (float) W - 68, (float) H);
        g.setColour (Colour (th.panel)); g.fillRect (face);
        g.setTiledImageFill (ed.brushed, 0, 0, 1.f); g.fillRect (face);
        g.drawImage (ed.woodL, Rectangle<float> (0, 0, 34, (float) H)); g.drawImage (ed.woodR, Rectangle<float> ((float) W - 34, 0, 34, (float) H));
        g.setColour (Colours::black.withAlpha (.4f)); g.fillRect (33.f, 0.f, 2.f, (float) H); g.fillRect ((float) W - 35, 0.f, 2.f, (float) H);
        for (auto pt : { Point<float> (44, 10), Point<float> ((float) W - 56, 10), Point<float> (44, (float) H - 22), Point<float> ((float) W - 56, (float) H - 22) }) {
            ColourGradient sc (Colour (0xff8b8e93), pt.x + 4, pt.y + 4, Colour (0xff2a2b2e), pt.x + 12, pt.y + 12, true); g.setGradientFill (sc); g.fillEllipse (pt.x, pt.y, 12, 12);
            g.setColour (Colour (0xff1c1d1f)); g.drawLine (pt.x + 2.5f, pt.y + 8, pt.x + 9.5f, pt.y + 4, 2); }
        g.setColour (Colour (th.logo)); g.setFont (ed.silk (36, true).withExtraKerningFactor (.32f)); g.drawText ("VIREO", Rectangle<float> (72, 20, 240, 40), Justification::centredLeft);
        g.setColour (Colour (th.sub)); g.setFont (ed.silk (11.5f).withExtraKerningFactor (.26f)); g.drawText ("VR-8 POLYPHONIC SYNTHESIZER", Rectangle<float> (72, 60, 300, 14), Justification::centredLeft);
        g.setColour (Colour (th.sub)); g.drawText ("FINISH", Rectangle<float> (310, 24, 100, 12), Justification::centredLeft);
        g.setColour (Colour (th.sub)); g.setFont (silkOf (ed, 11.f));
        g.drawText (CharPointer_UTF8 ("\xc2\xa9 2026 BXZEX  \xc2\xb7  BXZEX.COM"), Rectangle<float> (70, (float) H - 24, (float) W - 140, 14), Justification::centredRight);
        g.drawText (CharPointer_UTF8 ("VIREO VR-8  \xc2\xb7  DESIGNED AND BUILT BY BXZEX"), Rectangle<float> (70, (float) H - 24, (float) W - 140, 14), Justification::centredLeft);
        g.setColour (Colours::black.withAlpha (.5f)); g.fillRect (50.f, 84.f, (float) W - 100, 1.f);
        g.setColour (Colour (0xff17181a)); g.fillRoundedRectangle (54, 748, 90, 190, 7);
        g.setColour (Colour (0xff0d0d0e)); g.fillRoundedRectangle ((float) keys->getX() - 2, 748, (float) keys->getWidth() + 4, 190, 6);
        g.setColour (Colour (0xff6b1d1a)); g.fillRect ((float) keys->getX(), 750.f, (float) keys->getWidth(), 6.f);
    }
    VireoEditor& ed;
    std::vector<std::unique_ptr<Component>> owned; std::vector<std::unique_ptr<Section>> sections;
    std::vector<Section*> r1, r2, r3;
    std::unique_ptr<Screen> screen; std::unique_ptr<PresetLcd> lcd; std::unique_ptr<HwButton> prev, next; std::unique_ptr<Swatches> swatches;
    std::unique_ptr<Knob> vol; std::unique_ptr<Meter> meter; std::unique_ptr<Wheel> bendW, modW; std::unique_ptr<Keys> keys;
};

// ------------------------------------------------------------------ editor
VireoEditor::VireoEditor (VireoProcessor& p) : AudioProcessorEditor (&p), proc (p) {
    tfSemi = Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize);
    tfBold = Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedBold_ttf, BinaryData::BarlowCondensedBold_ttfSize);
    tfLcd = Typeface::createSystemTypefaceFor (BinaryData::ShareTechMonoRegular_ttf, BinaryData::ShareTechMonoRegular_ttfSize);
    brushed = makeBrushed();
    setTheme (jlimit (0, NUM_THEMES - 1, proc.theme));
    content = std::make_unique<Face> (*this); addAndMakeVisible (*content); content->setBounds (0, 0, W, H);
    setResizable (true, true); setResizeLimits (W / 2, H / 2, W * 3 / 2, H * 3 / 2); getConstrainer()->setFixedAspectRatio ((double) W / H);
    setSize ((int) (W * .8f), (int) (H * .8f));
    startTimerHz (5);
    setWantsKeyboardFocus (true);
    Timer::callAfterDelay (300, [safe = SafePointer<VireoEditor> (this)] { if (safe) if (auto* f = dynamic_cast<Face*> (safe->content.get())) f->keys->grabKeyboardFocus(); });
}
VireoEditor::~VireoEditor() = default;
const Theme& VireoEditor::theme() const { return THEMES[jlimit (0, NUM_THEMES - 1, proc.theme)]; }
Font VireoEditor::silk (float h, bool bold) const { return Font (FontOptions (bold ? tfBold : tfSemi).withHeight (h)); }
Font VireoEditor::lcd (float h) const { return Font (FontOptions (tfLcd).withHeight (h)); }
void VireoEditor::touched (int id) { statusId = id; statusT = Time::getMillisecondCounter(); }
void VireoEditor::setTheme (int t) {
    proc.theme = t; const auto& th = THEMES[t];
    woodL = makeWood (34, 960, 3, th.wood); woodR = makeWood (34, 960, 8, th.wood);
    if (content) content->repaint(); repaint();
}
void VireoEditor::refreshPresetName() { if (content) content->repaint(); }
void VireoEditor::paint (Graphics& g) { g.fillAll (Colour (0xff141414)); }
void VireoEditor::resized() { if (content) content->setTransform (AffineTransform::scale ((float) getWidth() / W)); }
bool VireoEditor::keyPressed (const KeyPress& k) { if (auto* f = dynamic_cast<Face*> (content.get())) return f->keys->keyPressed (k); return false; }
void VireoEditor::timerCallback() { static int last = -1; if (proc.program.load() != last) { last = proc.program.load(); refreshPresetName(); } }
