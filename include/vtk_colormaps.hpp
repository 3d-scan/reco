#ifndef VTK_COLORMAPS_NEW_HPP
#define VTK_COLORMAPS_NEW_HPP

#include <vtk_colormaps.h>
#include <vtk_colormaps_lut.hpp>

namespace utl
{
  //////////////////////////////////////////////////////////////////////////////
  void Colormap::setColormapType (COLORMAP_TYPE colormapType)
  {
    colormapType_ = colormapType;
  }
  
  //////////////////////////////////////////////////////////////////////////////
  void Colormap::setRangeLimits (double minVal, double maxVal)
  {
    minVal_ = minVal;
    maxVal_ = maxVal;
  }
  
  //////////////////////////////////////////////////////////////////////////////
  void Colormap::resetRangeLimits()
  {
    minVal_ = std::numeric_limits<double>::quiet_NaN();
    maxVal_ = std::numeric_limits<double>::quiet_NaN();
  }
  
  //////////////////////////////////////////////////////////////////////////////
  template <typename Scalar>
  void Colormap::setRangeLimitsFromData (const Eigen::Matrix<Scalar, 1, Eigen::Dynamic> &data)
  {
    if (data.size() > 0)
    {
      maxVal_ = static_cast<double>(data.maxCoeff());
      minVal_ = static_cast<double>(data.minCoeff());
    }
    else
    {
      maxVal_ = 0;
      minVal_ = 0;
    }      
  }
  
  //////////////////////////////////////////////////////////////////////////////
  template <typename Scalar>
  void Colormap::setRangeLimitsFromData (const std::vector<Scalar> &data)
  {
    if (data.size() > 0)
    {
      maxVal_ = static_cast<double>(*std::max_element(data.begin(), data.end()));
      minVal_ = static_cast<double>(*std::min_element(data.begin(), data.end()));
    }
    else
    {
      maxVal_ = 0;
      minVal_ = 0;
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  vtkSmartPointer<vtkLookupTable> Colormap::getColorLookupTable() const
  { 
    vtkSmartPointer<vtkLookupTable> colorLookupTable;
    
    // Generate lookuptable
    switch (colormapType_)
    {
      case VTK_DEFAULT:
        colorLookupTable = getLUT_VtkDefault();
        break;

      case GRAYSCALE:
        colorLookupTable = getLUT_Grayscale();
        break;
        
      case JET:
        colorLookupTable = getLUT_Jet();
        break;
        
      default:
        std::cout << "[utl::getColormap] Unknown colormap\n";
        return NULL;
        
    }
   
    // Set range
    colorLookupTable->SetTableRange(minVal_, maxVal_);
    
    return colorLookupTable;
  }
    
  //////////////////////////////////////////////////////////////////////////////
  template <typename Scalar>
  Colours Colormap::getColoursFromData(const Eigen::Matrix<Scalar, 1, Eigen::Dynamic> &data)
  {
    // Set range limits if they were not set before
    if (std::isnan(minVal_) || std::isnan(maxVal_))
      setRangeLimitsFromData<Scalar>(data);
    
    // Get lookup table
    vtkSmartPointer<vtkLookupTable> colourLUT = getColorLookupTable();
    
    // Get colours
    Colours colours;
    colours.resize(data.size());
    for (size_t i = 0; i < data.size(); i++)
    {
      double rgb[3];
      colourLUT->GetColor(data[i], rgb);
      Colour rgbVec (3);
      rgbVec[0] = rgb[0];
      rgbVec[1] = rgb[1];
      rgbVec[2] = rgb[2];
      colours[i] = rgbVec;
    }
    
    return colours;
  }  
  
  //////////////////////////////////////////////////////////////////////////////
  template <typename Scalar>
  Colours Colormap::getColoursFromData(const std::vector<Scalar> &data)
  {
    // Set range limits if they were not set before
    if (isnan(minVal_) || isnan(maxVal_))
      setRangeLimitsFromData<Scalar>(data);
    
    // Get lookup table
    vtkSmartPointer<vtkLookupTable> colourLUT = getColorLookupTable();
    
    // Get colours
    Colours colours;
    colours.resize(data.size());
    for (size_t i = 0; i < data.size(); i++)
    {
      double rgb[3];
      colourLUT->GetColor(data[i], rgb);
      Colour rgbVec (3);
      rgbVec[0] = rgb[0];
      rgbVec[1] = rgb[1];
      rgbVec[2] = rgb[2];
      colours[i] = rgbVec;
    }
    
    return colours;
  }

  //////////////////////////////////////////////////////////////////////////////
  vtkSmartPointer<vtkLookupTable> Colormap::getLUT_Jet () const
  {
    vtkSmartPointer<vtkLookupTable> colormap = vtkSmartPointer<vtkLookupTable>::New();    
    colormap->SetNumberOfTableValues(COLORMAP_NUM_DIVISIONS);

    for (size_t i = 0; i < COLORMAP_NUM_DIVISIONS; i++)
      colormap->SetTableValue(i,  JET_LUT[i][0], JET_LUT[i][1], JET_LUT[i][2]);
    
    return colormap;
  }
  
  //////////////////////////////////////////////////////////////////////////////
  vtkSmartPointer<vtkLookupTable> Colormap::getLUT_Grayscale () const
  {
    vtkSmartPointer<vtkLookupTable> colormap = vtkSmartPointer<vtkLookupTable>::New();
    colormap->SetNumberOfTableValues(COLORMAP_NUM_DIVISIONS);
    
    for (size_t i = 0; i < COLORMAP_NUM_DIVISIONS; i++)
    {
      double value = 1.0/(COLORMAP_NUM_DIVISIONS-1) * i;
      colormap->SetTableValue(i,  value, value, value);
    }    
    
    return colormap;
  }

  //////////////////////////////////////////////////////////////////////////////
  vtkSmartPointer<vtkLookupTable> Colormap::getLUT_VtkDefault () const
  {
    vtkSmartPointer<vtkLookupTable> colormap = vtkSmartPointer<vtkLookupTable>::New();
    colormap->Build();

    return colormap;
  }  
}

#endif // VTK_COLORMAPS_NEW_HPP