#include <mitsuba/render/integrator.h>
#include <mitsuba/render/records.h>

NAMESPACE_BEGIN(mitsuba)

/// This integrator wraps another integrator and returns its Stokes components as AOVs.
template <typename Float, typename Spectrum>
class StokesIntegrator final : public SamplingIntegrator<Float, Spectrum> {
public:
    MTS_DECLARE_CLASS_VARIANT(StokesIntegrator, SamplingIntegrator)
    MTS_IMPORT_BASE(SamplingIntegrator)
    MTS_IMPORT_TYPES(Scene, Sampler, SamplingIntegrator)

    StokesIntegrator(const Properties &props) : Base(props) {
        if constexpr (!is_polarized_v<Spectrum>)
            Throw("This integrator should only be used in polarized mode!");
        for (auto &kv : props.objects()) {
            SamplingIntegrator *integrator = dynamic_cast<SamplingIntegrator *>(kv.second.get());
            if (!integrator)
                Throw("Child objects must be of type 'SamplingIntegrator'!");
            if (m_integrator)
                Throw("More than one sub-integrator specified!");
            m_integrator = integrator;
        }

        if (!m_integrator)
            Throw("Must specify a sub-integrator!");
    }

    std::pair<Spectrum, Mask> sample(const Scene *scene,
                                     Sampler * sampler,
                                     const RayDifferential3f &ray,
                                     Float *aovs,
                                     Mask active) const override {
        auto result = m_integrator->sample(scene, sampler, ray, aovs + 12, active);

        if constexpr (is_polarized_v<Spectrum>) {
            auto const &stokes = result.first.coeff(0);
            for (int i = 0; i < 4; ++i) {
                Color3f rgb;
                if constexpr (is_monochromatic_v<Spectrum>) {
                    rgb = stokes[i].x();
                } else if constexpr (is_rgb_v<Spectrum>) {
                    rgb = stokes[i];
                } else {
                    static_assert(is_spectral_v<Spectrum>);
                    /// Note: this assumes that sensor used sample_rgb_spectrum() to generate 'ray.wavelengths'
                    auto pdf = pdf_rgb_spectrum(ray.wavelengths);
                    UnpolarizedSpectrum spec = stokes[i] * select(neq(pdf, 0.f), rcp(pdf), 0.f);
                    rgb = xyz_to_srgb(spectrum_to_xyz(spec, ray.wavelengths, active));
                }

                *aovs++ = rgb.r(); *aovs++ = rgb.g(); *aovs++ = rgb.b();
            }
        }

        return result;
    }

    std::vector<std::string> aov_names() const override {
        std::vector<std::string> result = m_integrator->aov_names();
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 3; ++j)
                result.insert(result.begin() + 3*i + j, "S" + std::to_string(i) + "." + ("RGB"[j]));
        return result;
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("integrator", m_integrator.get());
    }

private:
    ref<SamplingIntegrator> m_integrator;
};

MTS_EXPORT_PLUGIN(StokesIntegrator, "Stokes integrator");
NAMESPACE_END(mitsuba)
