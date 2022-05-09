#include <openvdb/openvdb.h>
#include <openvdb/tools/Interpolation.h>

#include <mitsuba/render/volume.h>
#include <mitsuba/core/mstream.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/mmap.h>

MTS_NAMESPACE_BEGIN

class OpenvdbDataSource : public VolumeDataSource {
 public:
  OpenvdbDataSource(const Properties &props) : VolumeDataSource(props) {
    openvdb::initialize();
    m_filename = props.getString("filename");
    m_gridname = props.getString("gridname", "");
    m_volumeToWorld = props.getTransform("toWorld", Transform());
    if (props.hasProperty("customStepSize")) {
      m_customStepSize = props.getFloat("customStepSize");
    }
    loadFromFile(m_filename);
  }

  OpenvdbDataSource(Stream *stream, InstanceManager *manager)
      : VolumeDataSource(stream, manager) {
    Log(EError, "Do not support serialization");
  }

  ~OpenvdbDataSource() override {}
  bool supportsFloatLookups() const override { return true; }
  bool supportsSpectrumLookups() const override { return false; }

  void loadFromFile(const fs::path &filename) {
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
        m_grid = openvdb::gridPtrCast<openvdb::FloatGrid>(baseGrid);
      }
    } else {
      Log(EError, "OpenVDB error: fail to open: %s", resolved.string().c_str());
    }
  }

  void configure() override {
    updateTransform();
    calculateBoundingBox();
    Float min_value, max_value;
    m_grid->evalMinMax(min_value, max_value);
    m_maximumFloatValue = max_value;
    Log(EDebug, "maximum float value: %f", m_maximumFloatValue);

    if (m_customStepSize == 0) {
      const openvdb::math::Vec3d &voxelSize =
          m_grid->constTransform().voxelSize();
      m_customStepSize =
          std::min(std::min(voxelSize.x(), voxelSize.y()), voxelSize.z()) * 0.5;
      Log(EDebug, "step size set to: %f", m_customStepSize);
    }
  }

  Float lookupFloat(const Point &p) const override {
    Point local = m_worldToVolume(p);
    openvdb::math::Vec3d gp(local.x, local.y, local.z);
    // gp = m_grid->transform().worldToIndex(gp);
    // Log(EDebug, "%f %f %f", gp.x(), gp.y(), gp.z());
    openvdb::tools::GridSampler<openvdb::FloatGrid::ConstAccessor,
                                openvdb::tools::BoxSampler>
        sampler(m_grid->getConstAccessor(), m_grid->transform());
    return sampler.isSample(gp);
  }

  Float getMaximumFloatValue() const override { return m_maximumFloatValue; }

  Float getStepSize() const override { return m_customStepSize; }

  MTS_DECLARE_CLASS()

 private:
  void updateTransform() {
    Matrix4x4 mat;
    openvdb::math::Mat4d g_mat =
        m_grid->transform().baseMap()->getAffineMap()->getMat4();
    for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 4; ++j) mat(i, j) = g_mat(i, j);

    m_grid->setTransform(openvdb::math::Transform::createLinearTransform(
        openvdb::math::Mat4d::identity()));
    mat = m_volumeToWorld.getMatrix() * mat;
    m_volumeToWorld = Transform(mat);
    m_worldToVolume = m_volumeToWorld.inverse();
  }

  void calculateBoundingBox() {
    openvdb::CoordBBox local_bounding_box =
        m_grid->evalActiveVoxelBoundingBox();
    openvdb::BBoxd bounding_box =
        m_grid->transform().indexToWorld(local_bounding_box);
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

  std::string m_filename;
  std::string m_gridname;
  openvdb::FloatGrid::Ptr m_grid{};
  Float m_customStepSize = 0;
  Transform m_volumeToWorld;
  Transform m_worldToVolume;
  Float m_maximumFloatValue;
};

MTS_IMPLEMENT_CLASS_S(OpenvdbDataSource, false, VolumeDataSource);
MTS_EXPORT_PLUGIN(OpenvdbDataSource, "OpenVDB data source");
MTS_NAMESPACE_END