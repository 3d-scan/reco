#ifndef PLANE_SEGMENTATION_MIN_CUT
#define PLANE_SEGMENTATION_MIN_CUT

// My includes
#include <adjacency_list.hpp>
#include <adjacency_list_operations.hpp>
#include <boost_min_cut.hpp>

// PCL includes
#include <pcl/common/angles.h>

namespace utl
{
  /** \brief Segment out points that belong to a planar model using min cut segmentation
    * \param[in] cloud input cloud
    * \param[in] plane_normal plane normal
    * \param[in] plane_point plane point
    * \param[out] plane_points segmented plane points
    */
  template <typename PointT>
  bool segmentPlanePoints ( const typename pcl::PointCloud<PointT>::ConstPtr &cloud,
                            const Eigen::VectorXf plane_coefficients,
                            std::vector<int> &plane_points
                          )
  {
    //----------------------------------------------------------------------------
    // Check input and convert plane parameters
    //----------------------------------------------------------------------------
    
    if (plane_coefficients.size() != 4)
    {
      std::cout << "[utl::segmentPlanePoints] plane coefficients must be length 4, instead they are length " << plane_coefficients.size() << "." << std::endl;
      return false;
    }
    
    Eigen::Vector3f planeNormal (plane_coefficients[0], plane_coefficients[1], plane_coefficients[2]);
    Eigen::Vector3f planePoint  ( - plane_coefficients[3] / plane_coefficients[0], 0.0f, 0.0f);
    
    planePoint = Eigen::projectPointToPlane(cloud->at(0).getVector3fMap(), planePoint, planeNormal);
    
    //----------------------------------------------------------------------------
    // Calculate unary potentials
    //----------------------------------------------------------------------------
    
    Eigen::Vector2f fgNormalAngleThresh (pcl::deg2rad(0.0),   pcl::deg2rad(10.0));
    Eigen::Vector2f bgNormalAngleThresh (pcl::deg2rad(30.0),  pcl::deg2rad(45.0));
    
    Eigen::Vector2f fgDistanceThresh (0.00f, 0.03f);
    Eigen::Vector2f bgDistanceThresh (0.05f, 0.2f);

    
    
    std::vector<float> fgWeights (cloud->size(), 0);
    std::vector<float> bgWeights (cloud->size(), 0);
    
    for (size_t pointId = 0; pointId < cloud->size(); pointId++)
    {
      // Get normal weight
      float curFgWeightNormal = 0;
      float curBgWeightNormal = 0;
      
      float dotProd = utl::clampValue(cloud->at(pointId).getNormalVector3fMap().dot(planeNormal), -1.0f, 1.0f);
      float normalAngleDiff = std::acos(dotProd);
      curFgWeightNormal = 1 - (utl::clampValue(normalAngleDiff, fgNormalAngleThresh[0], fgNormalAngleThresh[1]) - fgNormalAngleThresh[0]) / (fgNormalAngleThresh[1] - fgNormalAngleThresh[0]);
      curBgWeightNormal =     (utl::clampValue(normalAngleDiff, bgNormalAngleThresh[0], bgNormalAngleThresh[1]) - bgNormalAngleThresh[0]) / (bgNormalAngleThresh[1] - bgNormalAngleThresh[0]);
      
      // Get distance weight
      float curFgWeightDistance = 0;
      float curBgWeightDistance = 0;
      
      float distance = std::abs(Eigen::pointToPlaneDistance(cloud->at(pointId).getVector3fMap(), planePoint, planeNormal));
      curFgWeightDistance = 1 - (utl::clampValue(distance, fgDistanceThresh[0], fgDistanceThresh[1]) - fgDistanceThresh[0]) / (fgDistanceThresh[1] - fgDistanceThresh[0]);
      curBgWeightDistance =     (utl::clampValue(distance, bgDistanceThresh[0], bgDistanceThresh[1]) - bgDistanceThresh[0]) / (bgDistanceThresh[1] - bgDistanceThresh[0]);
      

      // Combine weights
      fgWeights[pointId] = curFgWeightNormal * curFgWeightDistance;
      bgWeights[pointId] = std::max(curBgWeightNormal, curBgWeightDistance);
      
//       if ((fgWeights[pointId] < 0) || (bgWeights[pointId] < 0) || std::isnan(fgWeights[pointId]) || std::isnan(bgWeights[pointId]))
//       {
//         cout << pointId << ", " << fgWeights[pointId] << ", " << bgWeights[pointId] << ", " << curFgWeightNormal << ", " << curBgWeightNormal << ", " << normalAngleDiff << ", " << distance << endl;
//         
//       }
    }
    
    //----------------------------------------------------------------------------
    // Find connectivity of the graph
    //----------------------------------------------------------------------------

    int numNeighbours = 6;
    
    pcl::search::KdTree<PointT> searchTree;
    searchTree.setInputCloud(cloud);  
    utl::Graph adjacency (cloud->size());
    
    for (size_t point1Id = 0; point1Id < cloud->size(); point1Id++)
    {
      // Find nearest neighbour
      std::vector<int> neighbours (numNeighbours);
      std::vector<float> distance (numNeighbours);    
      searchTree.nearestKSearch(point1Id, numNeighbours, neighbours, distance);
      
      for (size_t nbrIdIt = 1; nbrIdIt < neighbours.size(); nbrIdIt++)
        utl::addEdge(point1Id, neighbours[nbrIdIt], adjacency);
    }
      
    //----------------------------------------------------------------------------
    // Calcualte binary weights
    //----------------------------------------------------------------------------

    utl::GraphWeights binaryWeights = utl::createGraphWeights(adjacency);
    
    for (size_t point1Id = 0; point1Id < cloud->size(); point1Id++)
    {
      Eigen::Vector3f point1Normal = cloud->at(point1Id).getNormalVector3fMap();
      
      for (size_t point2It = 0; point2It < adjacency[point1Id].size(); point2It++)
      {
        int point2Id = adjacency[point1Id][point2It];
        Eigen::Vector3f point2Normal = cloud->at(point2Id).getNormalVector3fMap();
        
        float weight = (1.0 + point1Normal.dot(point2Normal)) / 2;
        utl::setEdgeWeight(adjacency, binaryWeights, point1Id, point2Id, weight);
      }
    }
    
    //----------------------------------------------------------------------------
    // Segment
    //----------------------------------------------------------------------------
    
    std::vector<int> fgPoints, bgPoints;
    boostMinCut(fgWeights, bgWeights, adjacency, binaryWeights, bgPoints, fgPoints);
    plane_points = fgPoints;
    
//     //----------------------------------------------------------------------------
//     // Debug visualization
//     //----------------------------------------------------------------------------
//     
//     PclVis visualizer = pcl::createVisualizer();
//     visualizer.setCameraPosition (  0.0, 0.0, 0.0,   // camera position
//                                     0.0, 0.0, 1.0,   // viewpoint
//                                     0.0, -1.0, 0.0,   // normal
//                                     0.0);            // viewport  
//   
//     utl::Colormap colormap;
//     pcl::showPointCloudWithData<PointT,float> (visualizer, cloud, fgWeights, colormap);
//     visualizer.addText("FG weights", 0, 50, 12, 1.0, 1.0, 1.0);
//     visualizer.spin();  
//   
//     visualizer.removeAllPointClouds();
//     visualizer.removeAllShapes();
//     pcl::showPointCloudWithData<PointT,float> (visualizer, cloud, bgWeights, colormap);
//     visualizer.addText("BG weights", 0, 50, 12, 1.0, 1.0, 1.0);
//     visualizer.spin();  
//     
//     visualizer.removeAllPointClouds();
//     pcl::showFGsegmentation(visualizer, *cloud, plane_points);
//     visualizer.spin();  
    
    return true;
  }
}

#endif    // PLANE_SEGMENTATION_MIN_CUT