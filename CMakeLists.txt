cmake_minimum_required(VERSION 3.20)
project(escalonadorHorarios LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_DESKTOP_GUI "Build Qt6 Desktop GUI" ON)

if(MSVC)
    add_compile_options(/utf-8)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

find_package(ortools REQUIRED)
find_package(highs QUIET)
find_package(scip QUIET)
find_package(nlohmann_json REQUIRED)

# Remove Boost test_exec_monitor that defines main/test_main
if(TARGET boost::boost)
    get_target_property(_boost_libs boost::boost INTERFACE_LINK_LIBRARIES)
    if(_boost_libs)
        list(FILTER _boost_libs EXCLUDE REGEX ".*exec_monitor.*")
        set_target_properties(boost::boost PROPERTIES INTERFACE_LINK_LIBRARIES "${_boost_libs}")
    endif()
endif()

include_directories(include)

set(CORE_SOURCES
    src/model/TimeSlot.cpp
    src/model/Professor.cpp
    src/model/Room.cpp
    src/model/Turma.cpp
    src/model/Lesson.cpp
    src/model/Timetable.cpp
    src/model/InstitutionData.cpp
    src/solver/Preprocessor.cpp
    src/solver/ScheduleSolver.cpp
    src/solver/CpSatSolver.cpp
    src/solver/GreedyHeuristicSolver.cpp
    src/solver/MilpSolver.cpp
    src/solver/ScipSolver.cpp
    src/solver/HighsSolver.cpp
    src/solver/CuOptSolver.cpp
    src/serialization/JsonSerialization.cpp
)

set(CORE_HEADERS
    include/model/TimeSlot.h
    include/model/Professor.h
    include/model/Room.h
    include/model/Turma.h
    include/model/Lesson.h
    include/model/OptimizationConfig.h
    include/model/Timetable.h
    include/model/InstitutionData.h
    include/solver/CandidateAllocation.h
    include/solver/Preprocessor.h
    include/solver/ScheduleSolver.h
    include/solver/SolverCommon.h
    include/solver/MilpModel.h
    include/serialization/JsonSerialization.h
)

add_library(scheduler_core STATIC ${CORE_SOURCES} ${CORE_HEADERS})

target_include_directories(scheduler_core PUBLIC include)

if(DEFINED highs_PACKAGE_FOLDER)
    target_include_directories(scheduler_core PUBLIC
        "${highs_PACKAGE_FOLDER}/include"
        "${highs_PACKAGE_FOLDER}/include/highs"
    )
elseif(DEFINED highs_PACKAGE_FOLDER_RELEASE)
    target_include_directories(scheduler_core PUBLIC
        "${highs_PACKAGE_FOLDER_RELEASE}/include"
        "${highs_PACKAGE_FOLDER_RELEASE}/include/highs"
    )
endif()

if(DEFINED scip_PACKAGE_FOLDER)
    target_include_directories(scheduler_core PUBLIC
        "${scip_PACKAGE_FOLDER}/include"
    )
elseif(DEFINED scip_PACKAGE_FOLDER_RELEASE)
    target_include_directories(scheduler_core PUBLIC
        "${scip_PACKAGE_FOLDER_RELEASE}/include"
    )
endif()

target_link_libraries(scheduler_core PUBLIC
    or-tools::or-tools
    highs::highs
    scip::scip
    nlohmann_json::nlohmann_json
    ${CMAKE_DL_LIBS}
)

add_executable(escalonador_solver_cli src/cli_main.cpp)
target_link_libraries(escalonador_solver_cli PRIVATE scheduler_core)

if(BUILD_DESKTOP_GUI)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)

    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

    set(UI_SOURCES
        src/main.cpp
        src/ui/ModernTheme.cpp
        src/ui/TimetableGridWidget.cpp
        src/ui/TimetableView.cpp
        src/ui/MoveLessonDialog.cpp
        src/ui/SwapLessonDialog.cpp
        src/ui/ChangeRoomDialog.cpp
        src/ui/AddLessonToScheduleDialog.cpp
        src/ui/ProfessorEditorDialog.cpp
        src/ui/TurmaEditorDialog.cpp
        src/ui/RoomEditorDialog.cpp
        src/ui/LessonEditorDialog.cpp
        src/ui/ConfigWidget.cpp
        src/ui/ScheduleReportExporter.cpp
        src/ui/TableSearchSortBar.cpp
        src/ui/OptimizeDialog.cpp
        src/ui/MainWindow.cpp
    )

    set(UI_HEADERS
        include/ui/ModernTheme.h
        include/ui/SmartTableWidgetItem.h
        include/ui/TableSearchSortBar.h
        include/ui/TimetableGridWidget.h
        include/ui/TimetableView.h
        include/ui/MoveLessonDialog.h
        include/ui/SwapLessonDialog.h
        include/ui/ChangeRoomDialog.h
        include/ui/AddLessonToScheduleDialog.h
        include/ui/ProfessorEditorDialog.h
        include/ui/TurmaEditorDialog.h
        include/ui/RoomEditorDialog.h
        include/ui/LessonEditorDialog.h
        include/ui/OptimizeDialog.h
        include/ui/ConfigWidget.h
        include/ui/ScheduleReportExporter.h
        include/ui/MainWindow.h
    )

    add_executable(escalonadorHorarios ${UI_SOURCES} ${UI_HEADERS})
    target_link_libraries(escalonadorHorarios PRIVATE
        scheduler_core
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
    )
endif()
