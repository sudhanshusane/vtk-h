#include "Renderer.hpp"
#include "Image.hpp"
#include "compositing/Compositor.hpp"

#include <vtkh/utils/vtkm_array_utils.hpp>
#include <vtkh/utils/vtkm_dataset_info.hpp>
#include <vtkh/utils/PNGEncoder.hpp>
#include <vtkm/rendering/raytracing/Logger.h>
#ifdef VTKH_PARALLEL
#include "compositing/DIYCompositor.hpp"
#endif

#include <assert.h>

namespace vtkh {

Renderer::Renderer()
  : m_do_composite(true),
    m_color_table("Cool to Warm"),
    m_field_index(0),
    m_has_color_table(true)
{
  m_compositor  = NULL; 
#ifdef VTKH_PARALLEL
  m_compositor  = new DIYCompositor(); 
#else
  m_compositor  = new Compositor(); 
#endif

}

Renderer::~Renderer()
{
  delete m_compositor;
}

void 
Renderer::SetField(const std::string field_name)
{
  m_field_name = field_name; 
}

std::string
Renderer::GetFieldName() const
{
  return m_field_name;
}

bool
Renderer::GetHasColorTable() const
{
  return m_has_color_table;
}

void 
Renderer::SetDoComposite(bool do_composite)
{
  m_do_composite = do_composite;
}

void
Renderer::AddRender(vtkh::Render &render)
{
  m_renders.push_back(render); 
}

void
Renderer::SetRenders(const std::vector<vtkh::Render> &renders)
{
  m_renders = renders; 
}

int
Renderer::GetNumberOfRenders() const
{
  return static_cast<int>(m_renders.size());
}

void
Renderer::ClearRenders()
{
  m_renders.clear(); 
}

void Renderer::SetColorTable(const vtkm::cont::ColorTable &color_table)
{
  m_color_table = color_table;
}

vtkm::cont::ColorTable Renderer::GetColorTable() const
{
  return m_color_table;
}

void 
Renderer::Composite(const int &num_images)
{

  m_compositor->SetCompositeMode(Compositor::Z_BUFFER_SURFACE);
  for(int i = 0; i < num_images; ++i)
  {
    const int num_canvases = m_renders[i].GetNumberOfCanvases();

    for(int dom = 0; dom < num_canvases; ++dom)
    {
      float* color_buffer = &GetVTKMPointer(m_renders[i].GetCanvas(dom)->GetColorBuffer())[0][0]; 
      float* depth_buffer = GetVTKMPointer(m_renders[i].GetCanvas(dom)->GetDepthBuffer()); 

      int height = m_renders[i].GetCanvas(dom)->GetHeight();
      int width = m_renders[i].GetCanvas(dom)->GetWidth();

      m_compositor->AddImage(color_buffer,
                             depth_buffer,
                             width,
                             height);
    } //for dom

    Image result = m_compositor->Composite();

#ifdef VTKH_PARALLEL
    if(vtkh::GetMPIRank() == 0)
    {
      ImageToCanvas(result, *m_renders[i].GetCanvas(0), true); 
    }
#else
    ImageToCanvas(result, *m_renders[i].GetCanvas(0), true); 
#endif
    m_compositor->ClearImages();
  } // for image
}

void 
Renderer::PreExecute() 
{
  if(!m_range.IsNonEmpty() && m_input->GlobalFieldExists(m_field_name))
  {
    // we have not been given a range, so ask the data set
    vtkm::cont::ArrayHandle<vtkm::Range> ranges = m_input->GetGlobalRange(m_field_name);
    int num_components = ranges.GetPortalControl().GetNumberOfValues();
    //
    // current vtkm renderers only supports single component scalar fields
    //
    assert(num_components == 1);
    if(num_components != 1)
    {
      std::stringstream msg;
      msg<<"Renderer '"<<this->GetName()<<"' cannot render a field with ";
      msg<<"'"<<num_components<<"' components. Field must be a scalar field.";
      throw Error(msg.str());
    }

    vtkm::Range global_range = ranges.GetPortalControl().Get(0);
    // a min or max may be been set by the user, check to see
    if(m_range.Min == vtkm::Infinity64())
    {
      m_range.Min = global_range.Min;
    }

    if(m_range.Max == vtkm::NegativeInfinity64())
    {
      m_range.Max = global_range.Max;
    }
  }

  m_bounds = m_input->GetGlobalBounds();
  
  // we may have to correct opacity to stay consistent 
  // if the number of samples in a volume rendering 
  // increased, but we still want annotations to reflect
  // the original color table values. If there is a 
  // correction, a derived class will set a new corrected
  // color table
  m_corrected_color_table = m_color_table;
}

void 
Renderer::Update() 
{
  PreExecute();
  DoExecute();
  PostExecute();
}

void 
Renderer::PostExecute() 
{
  int total_renders = static_cast<int>(m_renders.size());
  if(m_do_composite)
  {
    this->Composite(total_renders);
  }
}

void 
Renderer::DoExecute() 
{
  if(m_mapper.get() == 0)
  {
    std::string msg = "Renderer Error: no renderer was set by sub-class"; 
    throw Error(msg);
  }

  int total_renders = static_cast<int>(m_renders.size());
  int num_domains = static_cast<int>(m_input->GetNumberOfDomains());
  for(int dom = 0; dom < num_domains; ++dom)
  {
    vtkm::cont::DataSet data_set; 
    vtkm::Id domain_id;
    m_input->GetDomain(dom, data_set, domain_id);

    if(!data_set.HasField(m_field_name))
    {
      continue;
    }

    const vtkm::cont::DynamicCellSet &cellset = data_set.GetCellSet();
    const vtkm::cont::Field &field = data_set.GetField(m_field_name);
    const vtkm::cont::CoordinateSystem &coords = data_set.GetCoordinateSystem();
    if(cellset.GetNumberOfCells() == 0) continue;

    for(int i = 0; i < total_renders; ++i)
    {

      m_mapper->SetActiveColorTable(m_corrected_color_table);
      
      vtkmCanvasPtr p_canvas = m_renders[i].GetDomainCanvas(domain_id);
      const vtkmCamera &camera = m_renders[i].GetCamera(); 
      m_mapper->SetCanvas(&(*p_canvas));
      m_mapper->RenderCells(cellset,
                            coords,
                            field,
                            m_color_table,
                            camera,
                            m_range);

    }
  }


}

void 
Renderer::ImageToCanvas(Image &image, vtkm::rendering::Canvas &canvas, bool get_depth) 
{
  const int width = canvas.GetWidth(); 
  const int height = canvas.GetHeight(); 
  const int size = width * height;
  const int color_size = size * 4;
  float* color_buffer = &GetVTKMPointer(canvas.GetColorBuffer())[0][0]; 
  float one_over_255 = 1.f / 255.f;
#ifdef VTKH_USE_OPENMP
  #pragma omp parallel for 
#endif
  for(int i = 0; i < color_size; ++i)
  {
    color_buffer[i] = static_cast<float>(image.m_pixels[i]) * one_over_255;
  }

  float* depth_buffer = GetVTKMPointer(canvas.GetDepthBuffer()); 
  if(get_depth) memcpy(depth_buffer, &image.m_depths[0], sizeof(float) * size);
}

std::vector<Render> 
Renderer::GetRenders() const
{
  return m_renders;
}

vtkh::DataSet *
Renderer::GetInput() 
{
  return m_input;
}

vtkm::Range
Renderer::GetRange() const
{
  return m_range;
}

void
Renderer::SetRange(const vtkm::Range &range) 
{
  m_range = range;
}

} // namespace vtkh
