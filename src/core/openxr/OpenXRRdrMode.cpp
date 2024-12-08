// Software Name : SIBR_core
// SPDX-FileCopyrightText: Copyright (c) 2023 Orange
// SPDX-License-Identifier: Apache 2.0
//
// This software is distributed under the Apache 2.0 License;
// see the LICENSE file for more details.
//
// Author: Cédric CHEDALEUX <cedric.chedaleux@orange.com> et al.

#include <core/graphics/GUI.hpp>
#include <core/assets/Resources.hpp>
#include <core/openxr/OpenXRRdrMode.hpp>
#include <core/openxr/SwapchainImageRenderTarget.hpp>
#include <fstream>
#include <sstream>
#include <opencv2/opencv.hpp>

namespace sibr
{

#ifdef XR_USE_PLATFORM_XLIB
    XrGraphicsBindingOpenGLXlibKHR createXrGraphicsBindingOpenGLXlibKHR(Display *display, GLXDrawable drawable, GLXContext context)
    {
        return XrGraphicsBindingOpenGLXlibKHR{
            .type = XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR,
            .xDisplay = display,
            .glxDrawable = drawable,
            .glxContext = context};
    }
#endif

#ifdef XR_USE_PLATFORM_WIN32
    XrGraphicsBindingOpenGLWin32KHR createXrGraphicsBindingOpenGLWin32KHR(HDC hdc, HGLRC hglrc)
    {
        // Windows C++ compiler does not support C99 designated initializers
        return XrGraphicsBindingOpenGLWin32KHR{
            XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR, // .type
            NULL,                                      // .next
            hdc,                                       // .hDC
            hglrc                                      // .hGLRC
        };
    }
#endif

    OpenXRRdrMode::OpenXRRdrMode(sibr::Window &window, Eigen::Vector3f ipos, Eigen::Vector4f iq)
    {
        XRwindow = &window;
        m_quadShader.init("Texture",
                          sibr::loadFile(sibr::Resources::Instance()->getResourceFilePathName("texture.vp")),
                          sibr::loadFile(sibr::Resources::Instance()->getResourceFilePathName("texture.fp")));

        m_openxrHmd = std::make_unique<OpenXRHMD>("Gaussian splatting");
        m_openxrHmd->init();
        m_openxrHmd->setInitialPose(ipos, iq);
        bool sessionCreated = false;
#if defined(XR_USE_PLATFORM_XLIB)
        sessionCreated = m_openxrHmd->startSession(createXrGraphicsBindingOpenGLXlibKHR(glfwGetX11Display(), glXGetCurrentDrawable(), glfwGetGLXContext(window.GLFW())));
#elif defined(XR_USE_PLATFORM_WIN32)
        sessionCreated = m_openxrHmd->startSession(createXrGraphicsBindingOpenGLWin32KHR(wglGetCurrentDC(), wglGetCurrentContext()));
#endif
        if (!sessionCreated)
        {
            SIBR_ERR << "Failed to connect to OpenXR" << std::endl;
        }

        SIBR_LOG << "Disable VSync: use headset synchronization." << std::endl;
        window.setVsynced(false);

        m_openxrHmd->setIdleAppCallback([this]()
                                        { m_appFocused = false; });
        m_openxrHmd->setVisibleAppCallback([this]()
                                           { m_appFocused = false; });
        m_openxrHmd->setFocusedAppCallback([this]()
                                           { m_appFocused = true; });
        
        
        
    }

    OpenXRRdrMode::~OpenXRRdrMode()
    {/**/
        m_RTPool.clear();
        m_openxrHmd->closeSession();
        m_openxrHmd->terminate();
    }

     //Load Trace from CSV file  
    void OpenXRRdrMode::loadViewData(ViewData& view)
    { 
        //miss the first line header
        std::string header;
        std::getline(inFile, header);
        
        std::string line;
        //get the next line every frame
        if (std::getline(inFile, line)) {
            //using a stream to store the line
            std::istringstream lineStream(line);
            std::string token;

            //miss the ViewIndex
            std::getline(lineStream, token, ',');
            

            // Read FOV
            for (int i = 0; i < 4; ++i) {
                std::getline(lineStream, token, ',');
                view.fov(i) = std::stof(token);
            }

            // Read Position
            for (int i = 0; i < 3; ++i) {
                std::getline(lineStream, token, ',');
                view.position(i) = std::stof(token);
            }

            // Read Quaternion
            for (int i = 0; i < 4; ++i) {
                std::getline(lineStream, token, ',');
                view.quaternion.coeffs()(i) = std::stof(token);
            }
        }
        else {

            if (leftEyeVideoWriter.isOpened()) {
                leftEyeVideoWriter.release();
            }
            if (rightEyeVideoWriter.isOpened()) {
                rightEyeVideoWriter.release();
            }
            // Handle the end of file or read error
            PlayMode = 1;
            inFile.close();
            SIBR_LOG << "Replay Finished!" << std::endl;
            exit(0);
        }
    }


    void OpenXRRdrMode::render(ViewBase &view, const sibr::Camera &camera, const sibr::Viewport &viewport, IRenderTarget *optDest)
    {
        // Render the UI with OpenXR infos
        onGui();

        if (!m_openxrHmd->isSessionRunning())
        {
            return;
        }

        // Get next pose prediction for rendering
        m_openxrHmd->pollEvents();
        if (!m_openxrHmd->waitNextFrame())
        {
            return;
        }

        const int w = m_openxrHmd->getResolution().x();
        const int h = m_openxrHmd->getResolution().y();

        // Prepare the view to render at a specific resolution
        view.setResolution(sibr::Vector2i(w / m_downscaleResolution, h / m_downscaleResolution));

        // The callback is called for each single view (left view then right view) with the texture to render to
        m_openxrHmd->submitFrame([this, w, h, &view, &camera, optDest](int viewIndex, uint32_t texture)
                                 {
                                     OpenXRHMD::Eye eye = viewIndex == 0 ? OpenXRHMD::Eye::LEFT : OpenXRHMD::Eye::RIGHT;
                                    /*
                                     auto fov = this->m_openxrHmd->getFieldOfView(eye);
                                     auto q = this->m_openxrHmd->getPoseQuaternion(eye);
                                     auto pos = this->m_openxrHmd->getPosePosition(eye);
                                    */
                                     //Call loadViewData to replay
                                     
                                     ViewData viewData;
                                     if (PlayMode == 2) {
                                         loadViewData(viewData);
                                     }
                                     //playMode == 2, Replay; playMode == 0 or 1, HMD controls;
                                     auto fov = PlayMode == 2 ? viewData.fov : this->m_openxrHmd->getFieldOfView(eye);
                                     auto q = PlayMode == 2 ? viewData.quaternion : this->m_openxrHmd->getPoseQuaternion(eye);
                                     auto pos = PlayMode == 2 ? viewData.position : this->m_openxrHmd->getPosePosition(eye);

                                     if (viewIndex == 0) {
                                         otherEye = this->m_openxrHmd->getPosePosition(OpenXRHMD::Eye::RIGHT);
                                         Eigen::Vector3f gap = (otherEye - pos) / 2.0f;
                                         pos += gap * eyeScale;
                                     }
                                     else {
                                         otherEye = this->m_openxrHmd->getPosePosition(OpenXRHMD::Eye::LEFT);
                                         Eigen::Vector3f gap = (otherEye - pos) / 2.0f;
                                         pos += gap * eyeScale;
                                     }
                                     // OpenXR eye position is in world coordinates system (+x: right, +y: up; +z: backward)
                                     // 3DGS reference scenes have the following coordinate system : +x: right, +y: down, +z: forward
                                     // Let's rotate the camera to have the right-side up scene
                                     
                                    //-----save position and quaternion to file
                                    
                                     if (PlayMode == 0)
                                         outFile << viewIndex << "," // View index
                                         << fov.x() << "," << fov.y() << "," << fov.z() << "," << fov.w() << ","
                                         << pos.x() << "," << pos.y() << "," << pos.z() << ","
                                         << q.x() << "," << q.y() << "," << q.z() << "," << q.w() << "\n";
                                     
                                     if (true)
                                     {
                                         Eigen::Matrix3f mat;
                                         mat << 1.0f, 0.0f, 0.0f,
                                             0.0f, -1.0f, 0.0f,
                                             0.0f, 0.0f, -1.0f;
                                         Eigen::Matrix3f rot = mat * q.matrix();
                                         q = Eigen::Quaternionf(rot);
                                         pos = mat * pos;
                                     }



                                     // Define camera from OpenXR eye view position/orientation/fov
                                     Camera cam;
                                     
                                     cam.rotate(q);
                                     cam.position(pos);
                                     cam.zfar(0.2f);
                                     cam.znear(250.f);
                                     cam.fovy(fov.w() - fov.z());
                                     cam.aspect((fov.y() - fov.x()) / (fov.w() - fov.z()));

                                     if (m_vrExperience == 1) { // 1: seated experience - used sibr_viewer current camera's position as default position
                                         cam.translate(camera.position());
                                     }

                                     // Note: setStereoCam() used in SteroAnaglyph canno be reused here,
                                     // because headset eye views have asymetric fov
                                     // We therefore use the perspective() method with principal point positioning instead
                                     // 
                                     cam.principalPoint(Eigen::Vector2f(1.f, 1.f) - this->m_openxrHmd->getScreenCenter(eye));
                                     cam.perspective(cam.fovy(), (float)w / (float)h, cam.znear(), cam.zfar());

                                     // Get the render target holding the swapchain image's texture from the pool
                                     auto rt = getRenderTarget(texture, w, h);
                                     if (!rt)
                                     {
                                         return;
                                     }

                                     // Perform the scene rendering for the given view into the RenderTarget's FBO
                                     rt->clear();
                                     rt->bind();
                                     glViewport(0, 0, w, h);
                                     view.onRenderIBR(*rt.get(), cam);
                                     rt->unbind();

                                     // Draw the left and right textures into the UI window
                                     if (optDest)
                                     {
                                         glViewport(eye == OpenXRHMD::Eye::LEFT ? 0.f : optDest->w() / 2.f, 0, optDest->w() / 2.f, optDest->h());
                                         glScissor(eye == OpenXRHMD::Eye::LEFT ? 0.f : optDest->w() / 2.f, 0, optDest->w() / 2.f, optDest->h());
                                         optDest->bind();
                                     }
                                     else
                                     {
                                         glViewport(eye == OpenXRHMD::Eye::LEFT ? 0.f : w / 2.f, 0, w / 2.f, h);
                                         glScissor(eye == OpenXRHMD::Eye::LEFT ? 0.f : w / 2.f, 0, w / 2.f, h);
                                     }
                                     glEnable(GL_SCISSOR_TEST);
                                     glDisable(GL_BLEND);
                                     glDisable(GL_DEPTH_TEST);
                                     glClearColor(1.f, 0.f, 0.f, 1.f);
                                     glClear(GL_COLOR_BUFFER_BIT);
                                     m_quadShader.begin();
                                     glActiveTexture(GL_TEXTURE0);
                                     glBindTexture(GL_TEXTURE_2D, texture);
                                     RenderUtility::renderScreenQuad();
                                     glBindTexture(GL_TEXTURE_2D, 0);
                                     m_quadShader.end();
                                     glDisable(GL_SCISSOR_TEST);
                                     if (optDest)
                                     {
                                         optDest->unbind();
                                     }

                                     if (PlayMode==2) {
                                         glBindTexture(GL_TEXTURE_2D, texture);

                                         // create the buffer to store the data
                                         std::vector<GLubyte> pixels(w * h * 3);

                                         // load pixel data from GPU to buffer
                                         glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

                                         // convert pixel data to OpenCV Mat Object
                                         cv::Mat frame(h, w, CV_8UC3, pixels.data());

                                         // OpenCV default using BGR format，thus convert it to RGB
                                         cv::cvtColor(frame, frame, cv::COLOR_RGB2BGR);
                                          cv::flip(frame, frame, 0); 
                                         // write frame to different video according to the viewIndex(0 is left, 1 is right)
                                         if (viewIndex == 0 && leftEyeVideoWriter.isOpened()) {
                                             leftEyeVideoWriter.write(frame); // left eye
                                         }
                                         else if (viewIndex == 1 && rightEyeVideoWriter.isOpened()) {
                                             rightEyeVideoWriter.write(frame); // right eye
                                         }

                                         glBindTexture(GL_TEXTURE_2D, 0);
                                     }

                                 });
    }

    void OpenXRRdrMode::StartReplay(const std::string& saveFilePath) {
         
            const int w = m_openxrHmd->getResolution().x();
            const int h = m_openxrHmd->getResolution().y();
            int fourcc = cv::VideoWriter::fourcc('H', '2', '6', '4'); // using MJPG format
            int fps = 30; // 30 frame per second
            cv::Size frameSize(w, h); // video size
            size_t pos = saveFilePath.find_last_of(".");

            const std::string File= saveFilePath.substr(0, pos);
            // video file name is File+left/right
            leftEyeVideoWriter.open(File+"left.mp4", fourcc, fps, frameSize);
            rightEyeVideoWriter.open(File+"right.mp4", fourcc, fps, frameSize);

            if (true) {
                if (!inFile.is_open()) {
                    inFile.open(saveFilePath);
                    // Skip the header line
                    std::string header;
                    std::getline(inFile, header);
                }

                if (inFile.is_open()) {
                    PlayMode = 2;
                    SIBR_LOG << "Start Replay---by using the trace!" << std::endl;
                }
                else {
                    SIBR_LOG << "Do not find path file to replay!" << std::endl;
                }
            }
    }
    void OpenXRRdrMode::onGui()
    {
        const std::string guiName = "OpenXR";
        ImGui::Begin(guiName.c_str());
        std::string status = "KO";
        if (m_openxrHmd->isSessionRunning())
        {
            status = m_appFocused ? "FOCUSED" : "IDLE";
        }
        ImGui::Text("Session status: %s", status.c_str());
        ImGui::Text("Runtime: %s (%s)", m_openxrHmd->getRuntimeName().c_str(), m_openxrHmd->getRuntimeVersion().c_str());
        ImGui::Text("Reference space type: %s", m_openxrHmd->getReferenceSpaceType());
        ImGui::RadioButton("Free world standing", &m_vrExperience, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Seated", &m_vrExperience, 1);
        ImGui::Checkbox("Y-Invert scene", &m_flipY);

        // Add Save Tracks button
        if (ImGui::Button("Save Traces")) {
            PlayMode = 0;
            
            if (!outFile.is_open()) {
                const std::string& out = "Output" + std::to_string(FrameIndex++)+".csv";
                outFile.open(out, std::ios::app);
                // Write the CSV header if the file is being created
                if (outFile.tellp() == 0) {
                    outFile << "ViewIndex,FOV1,FOV2,FOV3,FOV4,PositionX,PositionY,PositionZ,QuaternionX,QuaternionY,QuaternionZ,QuaternionW\n";
                }
            }
            
            SIBR_LOG << "Start Saving HMD Tracks to output file" << std::endl;
        }

        // Add Stop Saving button
        ImGui::SameLine();  // Display on the same line
        if (ImGui::Button("Stop Saving")) {
            if (outFile.is_open()) {
                outFile.close();
            }
            PlayMode = 1;
            SIBR_LOG << "Saving Finished" << std::endl;
            
        }

        ImGui::SliderFloat("Scale Adjustment", &eyeScale,-1.0f,1.0f);

        if (m_openxrHmd->isSessionRunning())
        {
            const auto report = m_openxrHmd->getRefreshReport();
            ImGui::Text("Framerate: %.2f FPS (expected: %.2f FPS)", report.measuredFramerate, report.expectedFramerate);
            const auto w = m_openxrHmd->getResolution().x();
            const auto h = m_openxrHmd->getResolution().y();
            ImGui::Text("Headset resolution (per eye): %ix%i", w, h);
            ImGui::Text("Rendering resolution (per eye): %ix%i", w / m_downscaleResolution, h / m_downscaleResolution);
			ImGui::SliderInt("Down scale factor", &m_downscaleResolution, 1, 8);
            const auto leftPos = this->m_openxrHmd->getPosePosition(OpenXRHMD::Eye::LEFT);
            const auto rightPos = this->m_openxrHmd->getPosePosition(OpenXRHMD::Eye::RIGHT);
            const float eyeDist = sqrt(pow(leftPos.x() - rightPos.x(), 2) + pow(leftPos.y() - rightPos.y(), 2) + pow(leftPos.z() - rightPos.z(), 2));
            ImGui::Text("IPD: %.1fcm", eyeDist * 100.f);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenOverlapped))
            {
                ImGui::SetTooltip("Inter-pupillary distance");
            }
            if (ImGui::CollapsingHeader("Left eye:"))
            {
                const auto fov = m_openxrHmd->getFieldOfView(OpenXRHMD::Eye::LEFT, OpenXRHMD::AngleUnit::DEGREE);
                ImGui::Text("FOV: %.2f°, %.2f°, %.2f°, %.2f°", fov.x(), fov.y(), fov.z(), fov.w());
                ImGui::Text("Position : %.2f, %.2f, %.2f", leftPos.x(), leftPos.y(), leftPos.z());
            }
            if (ImGui::CollapsingHeader("Right eye:"))
            {
                const auto fov = m_openxrHmd->getFieldOfView(OpenXRHMD::Eye::RIGHT, OpenXRHMD::AngleUnit::DEGREE);
                ImGui::Text("FOV: %.2f°, %.2f°, %.2f°, %.2f°", fov.x(), fov.y(), fov.z(), fov.w());
                ImGui::Text("Position : %.2f, %.2f, %.2f", rightPos.x(), rightPos.y(), rightPos.z());
            }
        }
        ImGui::End();
    }

    SwapchainImageRenderTarget::Ptr OpenXRRdrMode::getRenderTarget(uint32_t texture, uint w, uint h)
    {
        auto i = m_RTPool.find(texture);
        if (i != m_RTPool.end())
        {
            return i->second;
        }
        else
        {
            SwapchainImageRenderTarget::Ptr newRt = std::make_shared<SwapchainImageRenderTarget>(texture, w, h);
            auto pair = m_RTPool.insert(std::make_pair<int, SwapchainImageRenderTarget::Ptr>(texture, std::move(newRt)));
            if (pair.second)
            {
                return (*pair.first).second;
            }
        }
        return SwapchainImageRenderTarget::Ptr();
    }

} /*namespace sibr*/