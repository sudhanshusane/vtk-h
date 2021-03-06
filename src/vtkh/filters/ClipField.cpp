#include "ClipField.hpp"

#include <vtkm/filter/ClipWithField.h>
#include <vtkh/filters/Recenter.hpp>

namespace vtkh 
{

ClipField::ClipField()
  : m_clip_value(0.0),
    m_invert(false)
{

}

ClipField::~ClipField()
{

}

void 
ClipField::SetClipValue(const vtkm::Float64 clip_value)
{
  m_clip_value = clip_value;
}

void 
ClipField::SetInvertClip(const bool invert)
{
  m_invert = invert;
}

void 
ClipField::SetField(const std::string field_name)
{
  m_field_name = field_name;
}

void 
ClipField::PreExecute() 
{
  Filter::PreExecute();
}

void 
ClipField::PostExecute()
{
  Filter::PostExecute();
}

void ClipField::DoExecute()
{
  
  this->m_output = new DataSet();

  const int num_domains = this->m_input->GetNumberOfDomains(); 

  vtkm::filter::ClipWithField clipper;
  clipper.SetClipValue(m_clip_value);
  clipper.SetInvertClip(m_invert);
  bool valid_field = false;
  bool is_cell_assoc = m_input->GetFieldAssociation(m_field_name, valid_field) ==
                       vtkm::cont::Field::Association::CELL_SET; 
  bool delete_input = false;
  if(valid_field && is_cell_assoc)
  {
    Recenter recenter;  
    recenter.SetInput(m_input);
    recenter.SetField(m_field_name);
    recenter.SetResultAssoc(vtkm::cont::Field::Association::POINTS); 
    recenter.Update();
    m_input = recenter.GetOutput();
    delete_input = true;
  }

  for(int i = 0; i < num_domains; ++i)
  {
    vtkm::Id domain_id;
    vtkm::cont::DataSet dom;
    this->m_input->GetDomain(i, dom, domain_id);

    if(!dom.HasField(m_field_name))
    {
      continue;
    }
    
    clipper.SetActiveField(m_field_name);
    clipper.SetFieldsToPass(this->GetFieldSelection());
    auto dataset = clipper.Execute(dom);
    this->m_output->AddDomain(dataset, domain_id);
    if(delete_input)
    {
      delete m_input;
    }
  }
}

std::string
ClipField::GetName() const
{
  return "vtkh::ClipField";
}

} //  namespace vtkh
