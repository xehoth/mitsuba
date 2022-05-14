#include <openvdb/openvdb.h>
#include <nanovdb/NanoVDB.h>
#include <nanovdb/util/GridHandle.h>
#include <nanovdb/util/OpenToNanoVDB.h>
#include <nanovdb/util/SampleFromVoxels.h>
#include <nanovdb/util/HDDA.h>
#include <nanovdb/util/Ray.h>

#include <mitsuba/render/volume.h>
#include <mitsuba/core/mstream.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/mmap.h>

MTS_NAMESPACE_BEGIN

class NanovdbDataSource : public VolumeDataSource {
 public:
  NanovdbDataSource(const Properties &props) : VolumeDataSource(props) {
    openvdb::initialize();
    m_filename = props.getString("filename");
    m_gridname = props.getString("gridname", "");
    m_volumeToWorld = props.getTransform("toWorld", Transform());
    if (props.hasProperty("customStepSize")) {
      m_customStepSize = props.getFloat("customStepSize");
    }
    loadFromFile(m_filename);
  }

  NanovdbDataSource(Stream *stream, InstanceManager *manager)
      : VolumeDataSource(stream, manager) {
    Log(EError, "Do not support serialization");
  }

  ~NanovdbDataSource() override {}
  bool supportsFloatLookups() const override { return true; }
  bool supportsSpectrumLookups() const override { return false; }

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
    m_maximumFloatValue = max_val;
    Log(EDebug, "maximum float value: %f", m_maximumFloatValue);
    updateTransform(openvdb_grid);
    calculateBoundingBox(openvdb_grid);
    m_gridHandle = std::make_shared<nanovdb::GridHandle<nanovdb::HostBuffer>>(
        nanovdb::openToNanoVDB(openvdb_grid));

    if (m_customStepSize == 0) {
      const openvdb::math::Vec3d &voxelSize =
          openvdb_grid->constTransform().voxelSize();
      m_customStepSize =
          static_cast<Float>(
              std::min(std::min(voxelSize.x(), voxelSize.y()), voxelSize.z())) *
          static_cast<Float>(0.5);
      Log(EDebug, "step size set to: %f", m_customStepSize);
    }
  }

  void configure() override {
    m_grid = m_gridHandle->grid<float>();
    if (m_grid == nullptr) Log(EError, "No grid specified!");
  }

  Float lookupFloat(const Point &p) const override {
    auto accessor = m_grid->getAccessor();
    auto sampler = nanovdb::createSampler<1>(accessor);
    Point local = m_worldToVolume(p);
    nanovdb::Vec3<Float> gp(local.x, local.y, local.z);
    return sampler(gp);
  }

  Float getMaximumFloatValue() const override { return m_maximumFloatValue; }

  Float getStepSize() const override { return m_customStepSize; }

  MTS_DECLARE_CLASS()

 private:
  void updateTransform(openvdb::FloatGrid::Ptr grid) {
    Matrix4x4 mat;
    openvdb::math::Mat4d g_mat =
        grid->transform().baseMap()->getAffineMap()->getMat4();
    for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 4; ++j) mat(i, j) = static_cast<Float>(g_mat(i, j));

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
    Point p_min(static_cast<Float>(b_min.x()), static_cast<Float>(b_min.y()),
                static_cast<Float>(b_min.z()));
    Point p_max(static_cast<Float>(b_max.x()), static_cast<Float>(b_max.y()),
                static_cast<Float>(b_max.z()));
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
  nanovdb::FloatGrid *m_grid{};
  std::shared_ptr<nanovdb::GridHandle<nanovdb::HostBuffer>> m_gridHandle{};
  Float m_customStepSize = 0;
  Transform m_volumeToWorld;
  Transform m_worldToVolume;
  Float m_maximumFloatValue;
};

MTS_IMPLEMENT_CLASS_S(NanovdbDataSource, false, VolumeDataSource);
MTS_EXPORT_PLUGIN(NanovdbDataSource, "NanoVDB data source");
MTS_NAMESPACE_END