#include <openvdb/openvdb.h>
#include <nanovdb/NanoVDB.h>
#include <nanovdb/util/GridHandle.h>
#include <nanovdb/util/OpenToNanoVDB.h>
#include <nanovdb/util/SampleFromVoxels.h>
#include <nanovdb/util/HDDA.h>
#include <nanovdb/util/Ray.h>

#include <mitsuba/render/scene.h>
#include <mitsuba/render/volume.h>
#include <mitsuba/core/statistics.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/properties.h>

MTS_NAMESPACE_BEGIN

class NanovdbMedium : public Medium {
 public:
  NanovdbMedium(const Properties &props) : Medium(props) {
    openvdb::initialize();
    m_scale = props.getFloat("scale", 1);
    m_filename = props.getString("filename");
    m_gridname = props.getString("gridname", "");
    m_volumeToWorld = props.getTransform("toWorld", Transform());
    loadFromFile(m_filename);
  }

  NanovdbMedium(Stream *stream, InstanceManager *manager)
      : Medium(stream, manager) {}

  void serialize(Stream *stream, InstanceManager *manager) const override {
    Log(EError, "Do not support serialization");
  }

  void configure() override {
    Medium::configure();
    m_density = m_densityHandle->grid<float>();
    if (m_albedo.get() == NULL) Log(EError, "No albedo specified!");
    if (m_density == nullptr) Log(EError, "No density specified!");
    m_anisotropicMedium =
        m_phaseFunction->needsDirectionallyVaryingCoefficients();
    if (m_anisotropicMedium && m_orientation.get() == NULL)
      Log(EError,
          "Cannot use anisotropic phase function: "
          "did not specify a particle orientation field!");
    if (m_anisotropicMedium) m_maxDensity *= m_phaseFunction->sigmaDirMax();
  }

  bool sampleDistance(const Ray &ray, MediumSamplingRecord &mRec,
                      Sampler *sampler) const override {
    mRec.pdfFailure = 1.0f;
    mRec.pdfSuccess = 1.0f;
    mRec.pdfSuccessRev = 1.0f;
    mRec.transmittance = Spectrum(1.0f);
    mRec.time = ray.time;

    Ray index_ray = m_worldToVolume(ray);
    nanovdb::Ray<Float> nano_index_ray(
        {index_ray.o.x, index_ray.o.y, index_ray.o.z},
        {index_ray.d.x, index_ray.d.y, index_ray.d.z}, index_ray.mint,
        index_ray.maxt);
    Float tMin, tMax;
    if (!nano_index_ray.intersects(m_density->indexBBox(), tMin, tMax))
      return false;
    nano_index_ray.setMaxTime(nanovdb::Min(tMax, nano_index_ray.t1()));
    nano_index_ray.setMinTime(nanovdb::Max(tMin, nano_index_ray.t0()));

    auto accessor = m_density->getAccessor();
    auto g_sampler = nanovdb::createSampler<1>(accessor);

    nanovdb::TreeMarcher<nanovdb::FloatTree::LeafNodeType, nanovdb::Ray<Float>,
                         nanovdb::FloatTree::AccessorType>
        marcher(accessor);
    marcher.init(nano_index_ray);

    const nanovdb::FloatTree::LeafNodeType *node;
    while (marcher.step(&node, tMin, tMax)) {
      Float t = tMin;
      Float mu_bar = m_maxDensity * m_scale;
      if (m_anisotropicMedium) mu_bar *= m_phaseFunction->sigmaDirMax();
      Float inv_mu_bar = 1 / mu_bar, mu = 0;
      int cnt = 0;
      while (true) {
        t -= math::fastlog(1 - sampler->next1D()) * inv_mu_bar;
        if (t >= tMax) break;
        if (++cnt > 100000) {
          Log(EDebug, "sample too many: %d", cnt);
        }
        mu = g_sampler(nano_index_ray(t)) * m_scale;

        if (mu * inv_mu_bar > sampler->next1D()) {
          Point p = m_volumeToWorld(index_ray(t));
          mRec.p = p;
          mRec.t = (mRec.p - ray.o).length() / ray.d.length();
          Spectrum albedo = m_albedo->lookupSpectrum(p);
          mRec.sigmaS = albedo * mu;
          mRec.sigmaA = Spectrum(mu) - mRec.sigmaS;
          mRec.transmittance = Spectrum(mu != 0.0f ? 1.0f / mu : 0);
          if (!std::isfinite(
                  mRec.transmittance[0]))  // prevent rare overflow warnings
            mRec.transmittance = Spectrum(0.0f);
          mRec.orientation = m_orientation != NULL
                                 ? m_orientation->lookupVector(p)
                                 : Vector(0.0f);
          mRec.medium = this;
          return true;
        }
      }
    }
    // Float mu_bar = 0;
    // for (Float t0 = tMin, t1 = tMax; marcher.step(&node, t0, t1);) {
    //   mu_bar = std::max(mu_bar, node->maximum());
    // }

    // mu_bar *= m_scale;
    // if (m_anisotropicMedium) mu_bar *= m_phaseFunction->sigmaDirMax();
    // Float inv_mu_bar = 1 / mu_bar;

    // Float t = tMin, mu = 0;
    // while (true) {
    //   t -= math::fastlog(1 - sampler->next1D()) * inv_mu_bar;
    //   if (t >= tMax) break;

    //   Point p = ray(t);
    //   mu = g_sampler(nano_index_ray(t)) * m_scale;

    //   if (mu * inv_mu_bar > sampler->next1D()) {
    //     mRec.p = m_volumeToWorld(index_ray(t));
    //     mRec.t = (mRec.p - ray.o).length() / ray.d.length();
    //     Spectrum albedo = m_albedo->lookupSpectrum(p);
    //     mRec.sigmaS = albedo * mu;
    //     mRec.sigmaA = Spectrum(mu) - mRec.sigmaS;
    //     mRec.transmittance = Spectrum(mu != 0.0f ? 1.0f / mu : 0);
    //     if (!std::isfinite(
    //             mRec.transmittance[0]))  // prevent rare overflow warnings
    //       mRec.transmittance = Spectrum(0.0f);
    //     mRec.orientation = m_orientation != NULL
    //                            ? m_orientation->lookupVector(p)
    //                            : Vector(0.0f);
    //     mRec.medium = this;
    //     return true;
    //   }
    // }

    // Float tr = 1;
    // while (marcher.step(&node, tMin, tMax)) {
    //   Float mu_bar = node->maximum() * m_scale;
    //   if (m_anisotropicMedium) mu_bar *= m_phaseFunction->sigmaDirMax();
    //   Float t = tMin;
    //   Float inv_mu_bar = 1 / mu_bar;
    //   Float weight = 1;
    //   for (;;) {
    //     t += -math::fastlog(1 - sampler->next1D()) * inv_mu_bar;
    //     if (t >= tMax) break;
    //     Float mu = g_sampler(index_ray(t)) * m_scale;
    //     tr *= 1 - mu * inv_mu_bar;
    //     if (mu * inv_mu_bar > sampler->next1D()) {
    //       // mRec.t =
    //       mRec.p = m_volumeToWorld(index_ray(t));
    //       mRec.t = (mRec.p - ray.o).length() / ray.d.length();
    //       Spectrum albedo = m_albedo->lookupSpectrum(mRec.p);
    //       mRec.sigmaS = albedo * mu;
    //       mRec.sigmaA = Spectrum(mu) - mRec.sigmaS;
    //       mRec.transmittance =
    //           Spectrum(mu != 0.0f ? 1.0f / mu : 0);
    //       if (!std::isfinite(
    //               mRec.transmittance[0]))  // prevent rare overflow warnings
    //         mRec.transmittance = Spectrum(0.0f);
    //       mRec.orientation = m_orientation != NULL
    //                              ? m_orientation->lookupVector(mRec.p)
    //                              : Vector(0.0f);
    //       mRec.medium = this;
    //       return true;
    //     }
    //   }
    // }
    return false;
  }

  void eval(const Ray &ray, MediumSamplingRecord &mRec) const override {
    Log(EError, "eval(): unsupported integration method!");
  }

  Spectrum evalTransmittance(const Ray &ray,
                             Sampler *sampler = NULL) const override {
    Ray index_ray = m_worldToVolume(ray);
    nanovdb::Ray<Float> nano_index_ray(
        {index_ray.o.x, index_ray.o.y, index_ray.o.z},
        {index_ray.d.x, index_ray.d.y, index_ray.d.z}, index_ray.mint,
        index_ray.maxt);
    Float tMin, tMax;
    if (!nano_index_ray.intersects(m_density->indexBBox(), tMin, tMax))
      return Spectrum(1.0);
    nano_index_ray.setMaxTime(nanovdb::Min(tMax, nano_index_ray.t1()));
    nano_index_ray.setMinTime(nanovdb::Max(tMin, nano_index_ray.t0()));

    auto accessor = m_density->getAccessor();
    auto g_sampler = nanovdb::createSampler<1>(accessor);
    nanovdb::TreeMarcher<nanovdb::FloatTree::LeafNodeType, nanovdb::Ray<Float>,
                         nanovdb::FloatTree::AccessorType>
        marcher(accessor);
    marcher.init(nano_index_ray);

    const nanovdb::FloatTree::LeafNodeType *node;
    Float tr = 1.0;
    while (marcher.step(&node, tMin, tMax)) {
      Float mu_bar = m_maxDensity * m_scale;
      if (m_anisotropicMedium) mu_bar *= m_phaseFunction->sigmaDirMax();
      Float t = tMin;
      Float inv_mu_bar = 1 / mu_bar;
      for (;;) {
        t += -math::fastlog(1 - sampler->next1D()) * inv_mu_bar;
        if (t >= tMax) break;
        Float density = g_sampler(nano_index_ray(t)) * m_scale;
        Float val = density * inv_mu_bar;
        if (val > 1) {
          Log(EDebug, "eval transmittance %f > 1", val);
          val = 1;
        }
        tr *= 1 - val;
      }
    }
    return Spectrum(tr);
  }

  bool isHomogeneous() const override { return false; }

  std::string toString() const override {
    std::ostringstream oss;
    oss << "NanovdbMedium[" << endl;
    return oss.str();
  }

  void addChild(const std::string &name, ConfigurableObject *child) override {
    if (child->getClass()->derivesFrom(MTS_CLASS(VolumeDataSource))) {
      VolumeDataSource *volume = static_cast<VolumeDataSource *>(child);
      if (name == "albedo") {
        Assert(volume->supportsSpectrumLookups());
        m_albedo = volume;
      } else if (name == "orientation") {
        Assert(volume->supportsVectorLookups());
        m_orientation = volume;
      } else {
        Medium::addChild(name, child);
      }
    } else {
      Medium::addChild(name, child);
    }
  }

  void loadFromFile(const fs::path &filename) {
    openvdb::FloatGrid::Ptr openvdb_grid{};
    {
      fs::path resolved =
          Thread::getThread()->getFileResolver()->resolve(filename);
      openvdb::io::File file(resolved.string());
      if (file.open()) {
        Log(EDebug, "Open vdb file: %s", resolved.string().c_str());
        openvdb::GridBase::Ptr baseGrid;
        if (m_gridname.empty()) {
          Log(EDebug, "Openvdb grid name not set, default use the first grid");
          baseGrid = file.readGrid(file.beginName().gridName());
        } else {
          for (openvdb::io::File::NameIterator iter = file.beginName();
               iter != file.endName(); ++iter) {
            if (iter.gridName() == m_gridname) {
              baseGrid = file.readGrid(iter.gridName());
            }
          }
        }
        if (!baseGrid->isType<openvdb::FloatGrid>()) {
          Log(EError, "Only support float grid");
        } else {
          openvdb_grid = openvdb::gridPtrCast<openvdb::FloatGrid>(baseGrid);
        }
      } else {
        Log(EError, "OpenVDB error: fail to open: %s",
            resolved.string().c_str());
      }
    }
    Float min_val, max_val;
    openvdb_grid->evalMinMax(min_val, max_val);
    m_maxDensity = max_val * m_scale;
    updateTransform(openvdb_grid);
    calculateBoundingBox(openvdb_grid);
    m_densityHandle =
        std::make_shared<nanovdb::GridHandle<nanovdb::HostBuffer>>(
            nanovdb::openToNanoVDB(openvdb_grid));
  }

  MTS_DECLARE_CLASS()
 protected:
  void updateTransform(openvdb::FloatGrid::Ptr grid) {
    Matrix4x4 mat;
    openvdb::math::Mat4d g_mat =
        grid->transform().baseMap()->getAffineMap()->getMat4();
    for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 4; ++j) mat(i, j) = g_mat(i, j);

    grid->setTransform(openvdb::math::Transform::createLinearTransform(
        openvdb::math::Mat4d::identity()));
    mat = m_volumeToWorld.getMatrix() * mat;
    m_volumeToWorld = Transform(mat);
    m_worldToVolume = m_volumeToWorld.inverse();
  }

  void calculateBoundingBox(openvdb::FloatGrid::Ptr grid) {
    openvdb::CoordBBox local_bounding_box = grid->evalActiveVoxelBoundingBox();
    openvdb::BBoxd bounding_box =
        grid->transform().indexToWorld(local_bounding_box);
    auto b_min = bounding_box.min();
    auto b_max = bounding_box.max();
    Log(EDebug, "local bounding box: (%f, %f, %f) x (%f, %f, %f)", b_min.x(),
        b_min.y(), b_min.z(), b_max.x(), b_max.y(), b_max.z());
    Point p_min(b_min.x(), b_min.y(), b_min.z());
    Point p_max(b_max.x(), b_max.y(), b_max.z());
    p_min = m_volumeToWorld(p_min);
    p_max = m_volumeToWorld(p_max);
    m_aabb.reset();
    m_aabb.expandBy(p_min);
    m_aabb.expandBy(p_max);

    Log(EDebug, "world bounding box: (%f, %f, %f) x (%f, %f, %f)", p_min.x,
        p_min.y, p_min.z, p_max.x, p_max.y, p_max.z);
  }

  ref<VolumeDataSource> m_albedo;
  ref<VolumeDataSource> m_orientation;
  Float m_scale;
  bool m_anisotropicMedium;
  std::string m_filename;
  std::string m_gridname;
  nanovdb::FloatGrid *m_density{};
  std::shared_ptr<nanovdb::GridHandle<nanovdb::HostBuffer>> m_densityHandle{};
  AABB m_aabb;
  Transform m_volumeToWorld;
  Transform m_worldToVolume;
  Float m_maxDensity;
};

MTS_IMPLEMENT_CLASS_S(NanovdbMedium, false, Medium)
MTS_EXPORT_PLUGIN(NanovdbMedium, "NanoVDB medium");
MTS_NAMESPACE_END