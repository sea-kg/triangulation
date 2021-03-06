#include <iostream>
#include <vector>
#include <fstream>
#include <iomanip>

#include "render_window.h"
#include "wsjcpp_core.h"
#include "sea5kg_triangulation.h"

void trinagulateAndUpdate(
    std::vector<RenderTriangle *> &vRenderingTriangles,
    std::vector<RenderArea *> &vRenderingAreas,
    Sea5kgTriangulationTriangulator *pTriangulator,
    RenderWindow &window
) {
    for (int i = 0; i < vRenderingTriangles.size(); i++) {
        window.removeObject(vRenderingTriangles[i]);
    }
    vRenderingTriangles.clear();

    nlohmann::json jsonAreas = nlohmann::json::array();

    std::vector<Sea5kgTriangulationArea> &vAreas = pTriangulator->getAreas();
    for (int i = 0; i < vRenderingAreas.size(); i++) {
        nlohmann::json jsonArea;
        jsonArea["id"] = vAreas[i].getId();
        jsonArea["number-of-triangels"] = vAreas[i].getCountTriangles();
        vAreas[i].clear();
        nlohmann::json jsonPoints = nlohmann::json::array();
        std::vector<CoordXY> vPoints = vRenderingAreas[i]->getPoints();
        for (int x = 0; x < vPoints.size(); x++) {
           vAreas[i].addPoint(vPoints[x].x(), vPoints[x].y());
           nlohmann::json jsonPoint;
           jsonPoint["x"] = vPoints[x].x();
           jsonPoint["y"] = vPoints[x].y();
           jsonPoints.push_back(jsonPoint);
        }
        jsonArea["points"] = jsonPoints;
        jsonAreas.push_back(jsonArea);
    }
    nlohmann::json jsonInput;
    jsonInput["areas"] = jsonAreas;

    std::ofstream file("./input.json");
    file << std::setw(4) << jsonInput;

    pTriangulator->triangulate();
    
    RenderColor triangleColor(0,255,255,190);
    const std::vector<Sea5kgTriangulationTriangle> &vTriangles = pTriangulator->getTriangles();
    for (int i = 0; i < vTriangles.size(); i++) {
        Sea5kgTriangulationTriangle tr = vTriangles[i];
        auto *pTriangle = new RenderTriangle(
            CoordXY(tr.p1.getX(), tr.p1.getY()),
            CoordXY(tr.p2.getX(), tr.p2.getY()),
            CoordXY(tr.p3.getX(), tr.p3.getY()),
            triangleColor
        );
        vRenderingTriangles.push_back(pTriangle);
        window.addObject(pTriangle);
    }
}

int main(int argc, char* args[]) {

    if (SDL_Init(SDL_INIT_VIDEO) > 0) {
        std::cerr << "HEY.. SDL_Init HAS FAILED. SDL_ERROR: " << SDL_GetError() << std::endl;
        return -1;
    }

    if (TTF_Init() == -1) {
        printf("TTF_Init: %s\n", TTF_GetError());
        return -1;
    }

    int nWindowWidth = 1280;
    int nWindowHeight = 720;
    CoordXY centerPoint(nWindowWidth/2, nWindowHeight/2);
    Sea5kgTriangulationTriangulator *pTriangulator = new Sea5kgTriangulationTriangulator();

    AppState appState(nWindowWidth, nWindowHeight);

    RenderWindow window(
        "sea5kg-triangulation (v0.0.2)",
        appState.windowWidth(),
        appState.windowHeight()
    );

    // load map from json
    std::ifstream ifs("./input.json");
    nlohmann::json jf = nlohmann::json::parse(ifs);

    nlohmann::json jsonAreas = jf["areas"];
    for (auto& el : jsonAreas.items()) {
        std::string sAreaId = el.value()["id"];
        int nNumberOfTriangles = el.value()["number-of-triangels"];
        std::cout << "Load area " << sAreaId << std::endl;
        Sea5kgTriangulationArea ar(sAreaId);
        ar.setCountTriangles(nNumberOfTriangles);
        nlohmann::json jsonPoints = el.value()["points"];
        for (auto& pt : jsonPoints.items()) {
            int nX = pt.value()["x"];
            int nY = pt.value()["y"];
            ar.addPoint(nX, nY);
        }
        pTriangulator->addArea(ar);
    }

    std::vector<RenderArea *> vRenderingAreas;
    RenderRect *pMoveRect = nullptr;
    // add to renderer
    RenderColor areaColor(255,255,255,255);

    const std::vector<Sea5kgTriangulationArea> &vAreas = pTriangulator->getAreas();
    for (int i = 0; i < vAreas.size(); i++) {
        Sea5kgTriangulationArea ar = vAreas[i];
        std::vector<CoordXY> vPoints;
        for (int x = 0; x < ar.count(); x++) {
            CoordXY p0(ar.getPoint(x).getX(),ar.getPoint(x).getY());
            vPoints.push_back(p0);
        }
        RenderArea *pArea = new RenderArea(vPoints, areaColor);
        vRenderingAreas.push_back(pArea);
        window.addObject(pArea);
    }

    // pTriangulator->triangulate();
    
    std::vector<RenderTriangle *> vRenderingTriangles;

    trinagulateAndUpdate(vRenderingTriangles, vRenderingAreas, pTriangulator, window);

    RenderAbsoluteTextBlock *pFpsText = new RenderAbsoluteTextBlock(CoordXY(50,20), "FPS: ----", 1000);
    window.addObject(pFpsText);

    RenderColor cursorPointer(255,0,0,190);
    RenderMouse *pMouse = new RenderMouse(centerPoint, cursorPointer, 2000);
    window.addObject(pMouse);

    window.sortObjectsByPositionZ();

    bool appRunning = true;

    SDL_Event event;

    long nNumberOfFrames = 0;
    long nStartTime = WsjcppCore::getCurrentTimeInMilliseconds();
    long nElapsed = 0;
    appState.init();
    while (appRunning) {
        // Get our controls and events
        while (SDL_PollEvent(&event)) {
            switch(event.type) {
                case SDL_QUIT:
                    appRunning = false;
                    break;
                case SDL_MOUSEMOTION:
                    if (appState.isMouseCaptured()) {
                        CoordXY p0(event.motion.x, event.motion.y);
                        pMouse->updateCoord(p0);
                        if (pMoveRect != nullptr) {
                            for (int i = 0; i < vRenderingAreas.size(); i++) {
                                vRenderingAreas[i]->updatePointCoord(pMoveRect, p0);
                            }
                        } else {
                            bool bArrow = true;
                            for (int i = 0; i < vRenderingAreas.size(); i++) {
                                RenderRect *pFoundRect = nullptr;
                                if (vRenderingAreas[i]->hasMoveblePoint(p0, pFoundRect)) {
                                    pMouse->changeCursorToMoveble();
                                    bArrow = false;
                                }
                            }
                            if (bArrow) {
                                pMouse->changeCursorToArrow();
                            }
                        }
                    }
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (!appState.isMouseCaptured()) {
                        appState.setMouseCaptured(true);
                    } else {
                        bool bMove = false;
                        CoordXY p0(event.motion.x, event.motion.y);
                        for (int i = 0; i < vRenderingAreas.size(); i++) {
                            RenderRect *pFoundRect = nullptr;
                            if (vRenderingAreas[i]->hasMoveblePoint(p0, pFoundRect)) {
                                pMoveRect = pFoundRect;
                                bMove = true;
                            }
                        }
                        if (!bMove) {
                            pMoveRect = nullptr;
                        }
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (pMoveRect != nullptr) {
                        pMoveRect = nullptr;
                        trinagulateAndUpdate(vRenderingTriangles, vRenderingAreas, pTriangulator, window);
                    }
                    break;
                case SDL_KEYDOWN:
                    switch(event.key.keysym.sym) {
                        case SDLK_ESCAPE: 
                            if (appState.isMouseCaptured()) {
                                appState.setMouseCaptured(false);
                            }
                            break;
                    }
                    break;
                default:
                    break; // nothing
            }
        }

        appState.updateElapsedTime();
        window.clear();
        window.modifyObjects(appState);
        window.drawObjects();

        // FPS
        nNumberOfFrames++;
        nElapsed = WsjcppCore::getCurrentTimeInMilliseconds() - nStartTime;
        if (nElapsed > 3000) {
            double nFPS = nNumberOfFrames;
            nFPS /= nElapsed;
            nFPS *= 1000;
            std::cout << "FPS: " << nFPS << std::endl;
            nStartTime = WsjcppCore::getCurrentTimeInMilliseconds();
            nNumberOfFrames = 0;
            pFpsText->updateText("FPS: " + std::to_string(int(nFPS)));
        }
    }

    window.cleanUp();
    SDL_Quit();
    return 0;
}