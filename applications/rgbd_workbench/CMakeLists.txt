set(_module rgbd_workbench)

reco_add_subproject(${_module}
    DEPENDENCIES datapipe utils OpenCV HAL VTK PCL
    APPLICATION QT)