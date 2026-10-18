#include <SDL3/SDL.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <handkit/input.h>
#include "demo_modes.h"
#include <handkit/runtime.h>
#include <handkit/system.h>
#ifndef DEMO_ENTRY
#define DEMO_ENTRY main
#endif

static volatile sig_atomic_t running = 1;

static void stop(int sig) {
    (void)sig;
    running = 0;
#ifdef DEMO_COMBINED
    demo_shutdown = 1;
#endif
}

static SDL_GPUShader *load_shader(SDL_GPUDevice *gpu, const char *file, SDL_GPUShaderStage stage, Uint32 uniforms, Uint32 samplers) {
    char path[1024];
    SDL_snprintf(path, sizeof(path), "%sassets/shaders/%s.spv", SDL_GetBasePath(), file);
    size_t size = 0;
    void *code = SDL_LoadFile(path, &size);
    if (!code) {
        return NULL;
    }
    SDL_GPUShaderCreateInfo info = {.code_size = size, .code = code, .entrypoint = "main", .format = SDL_GPU_SHADERFORMAT_SPIRV, .stage = stage, .num_uniform_buffers = uniforms, .num_samplers = samplers};
    SDL_GPUShader *result = SDL_CreateGPUShader(gpu, &info);
    SDL_free(code);
    return result;
}

int DEMO_ENTRY(int argc, char **argv) {
    running = 1;
    int mode_request = DEMO_EXIT;
    unsigned duration = argc > 1 ? (unsigned)atoi(argv[1]) : 0;
    signal(SIGTERM, stop);
    signal(SIGINT, stop);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "handkit");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    int status = 1;
    SDL_Window *window = NULL;
    SDL_GPUDevice *gpu = NULL;
    SDL_GPUGraphicsPipeline *cube = NULL;
    SDL_GPUGraphicsPipeline *hud = NULL;
    SDL_GPUShader *cube_vertex_shader = NULL;
    SDL_GPUShader *cube_fragment_shader = NULL;
    SDL_GPUShader *hud_vertex_shader = NULL;
    SDL_GPUShader *hud_fragment_shader = NULL;
    SDL_GPUTexture *color = NULL;
    SDL_GPUTexture *depth = NULL;
    SDL_GPUTexture *hudtex = NULL;
    SDL_GPUTransferBuffer *upload = NULL;
    SDL_GPUTransferBuffer *download = NULL;
    SDL_GPUSampler *sampler = NULL;
    SDL_Surface *text = NULL;
    SDL_Renderer *text_renderer = NULL;
    HkInput input;
    hk_input_init(&input);
    bool claimed = false;
    if (!hk_init(HK_BACKEND_VULKAN)) {
        goto done;
    }
    /* Start + Select, the power button and the volume bar, drawn into the HUD. */
    hk_system_init();
    const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());
    if (!mode) {
        goto done;
    }
    Uint32 w = mode->w;
    Uint32 h = mode->h;
    window = SDL_CreateWindow("SDL3 Vulkan 3D", w, h, SDL_WINDOW_VULKAN | SDL_WINDOW_FULLSCREEN);
    if (!window) {
        goto done;
    }
    gpu = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, "vulkan");
    if (!gpu) {
        goto done;
    }
    fprintf(stderr, "SDL %d GPU driver=%s size=%ux%u\n", SDL_GetVersion(), SDL_GetGPUDeviceDriver(gpu), w, h);
    if (!SDL_ClaimWindowForGPUDevice(gpu, window)) {
        goto done;
    }
    claimed = true;
    if (!SDL_SetGPUSwapchainParameters(gpu, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC)) {
        goto done;
    }
    SDL_GPUTextureFormat format = SDL_GetGPUSwapchainTextureFormat(gpu, window);
    cube_vertex_shader = load_shader(gpu, "cube.vert", SDL_GPU_SHADERSTAGE_VERTEX, 1, 0);
    cube_fragment_shader = load_shader(gpu, "cube.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0);
    hud_vertex_shader = load_shader(gpu, "hud.vert", SDL_GPU_SHADERSTAGE_VERTEX, 0, 0);
    hud_fragment_shader = load_shader(gpu, "hud.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 1);
    if (!cube_vertex_shader || !cube_fragment_shader || !hud_vertex_shader || !hud_fragment_shader) {
        goto done;
    }
    SDL_GPUTextureFormat depth_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
    if (!SDL_GPUTextureSupportsFormat(gpu, depth_format, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)) {
        depth_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    }
    SDL_GPUColorTargetDescription target = {.format = format};
    SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {.vertex_shader = cube_vertex_shader,
                                                       .fragment_shader = cube_fragment_shader,
                                                       .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
                                                       .rasterizer_state = {.fill_mode = SDL_GPU_FILLMODE_FILL, .cull_mode = SDL_GPU_CULLMODE_NONE, .enable_depth_clip = true},
                                                       .multisample_state = {.sample_count = SDL_GPU_SAMPLECOUNT_1},
                                                       .depth_stencil_state = {.compare_op = SDL_GPU_COMPAREOP_LESS, .enable_depth_test = true, .enable_depth_write = true},
                                                       .target_info = {.color_target_descriptions = &target, .num_color_targets = 1, .depth_stencil_format = depth_format, .has_depth_stencil_target = true}};
    cube = SDL_CreateGPUGraphicsPipeline(gpu, &pipeline_info);
    if (!cube) {
        goto done;
    }
    pipeline_info.vertex_shader = hud_vertex_shader;
    pipeline_info.fragment_shader = hud_fragment_shader;
    pipeline_info.depth_stencil_state.enable_depth_test = false;
    pipeline_info.depth_stencil_state.enable_depth_write = false;
    target.blend_state = (SDL_GPUColorTargetBlendState){.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                                                        .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                                                        .color_blend_op = SDL_GPU_BLENDOP_ADD,
                                                        .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
                                                        .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                                                        .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
                                                        .enable_blend = true};
    hud = SDL_CreateGPUGraphicsPipeline(gpu, &pipeline_info);
    if (!hud) {
        goto done;
    }
    SDL_GPUTextureCreateInfo texture_info = {
        .type = SDL_GPU_TEXTURETYPE_2D, .format = format, .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER, .width = w, .height = h, .layer_count_or_depth = 1, .num_levels = 1, .sample_count = SDL_GPU_SAMPLECOUNT_1};
    color = SDL_CreateGPUTexture(gpu, &texture_info);
    texture_info.format = depth_format;
    texture_info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    depth = SDL_CreateGPUTexture(gpu, &texture_info);
    texture_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texture_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    hudtex = SDL_CreateGPUTexture(gpu, &texture_info);
    sampler = SDL_CreateGPUSampler(gpu, &(SDL_GPUSamplerCreateInfo){.min_filter = SDL_GPU_FILTER_NEAREST,
                                                                    .mag_filter = SDL_GPU_FILTER_NEAREST,
                                                                    .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
                                                                    .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
                                                                    .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
                                                                    .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE});
    upload = SDL_CreateGPUTransferBuffer(gpu, &(SDL_GPUTransferBufferCreateInfo){.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = w * h * 4});
    text = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
    if (text) {
        text_renderer = SDL_CreateSoftwareRenderer(text);
    }
    if (!color || !depth || !hudtex || !sampler || !upload || !text_renderer) {
        goto done;
    }
    int count = 0;
    SDL_JoystickID *ids = SDL_GetJoysticks(&count);
    fprintf(stderr, "Controller=%s\n", count ? SDL_GetJoystickNameForID(ids[0]) : "none");
    SDL_free(ids);
    Uint64 start = SDL_GetTicks();
    Uint64 prev = start;
    Uint64 fps_start = start;
    Uint64 hud_time = 0;
    unsigned frames = 0;
    unsigned fps_frames = 0;
    float fps = 0;
    float rx = -0.35f;
    float ry = 0.6f;
    float distance = 4.2f;
    bool paused = false;
    const char *capture = SDL_getenv("HANDKIT_DEMO_CAPTURE");
    const char *capture_at = SDL_getenv("HANDKIT_DEMO_CAPTURE_FRAME");
    unsigned capture_frame = capture_at ? (unsigned)SDL_atoi(capture_at) : 90;
    while (running && (!duration || SDL_GetTicks() - start < duration * 1000ULL)) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                running = 0;
            }
            if (e.type == SDL_EVENT_JOYSTICK_BUTTON_DOWN) {
                hud_time = 0;
            }
        }
        hk_input_update(&input);
        hk_system_update(&input);
#ifdef DEMO_COMBINED
        if (input.released[HK_BUTTON_X]) {
            mode_request = DEMO_NEXT;
            running = 0;
        }
        if (input.released[HK_BUTTON_Y]) {
            mode_request = DEMO_PREVIOUS;
            running = 0;
        }
#endif
        if (input.pressed[HK_BUTTON_A]) {
            paused = !paused;
        }
        if (input.pressed[HK_BUTTON_B]) {
            rx = -.35f;
            ry = .6f;
            distance = 4.2f;
        }
        Uint64 now = SDL_GetTicks();
        float dt = SDL_min((now - prev) / 1000.0f, 0.1f);
        prev = now;
        if (!paused) {
            ry += dt * .6f;
        }
        ry += (!!(input.dpad & SDL_HAT_RIGHT) - !!(input.dpad & SDL_HAT_LEFT) + input.left_x) * dt * 1.8f;
        rx += (!!(input.dpad & SDL_HAT_DOWN) - !!(input.dpad & SDL_HAT_UP) + input.left_y) * dt * 1.8f;
        if (input.held[HK_BUTTON_L1]) {
            distance += dt * 2;
        }
        if (input.held[HK_BUTTON_R1]) {
            distance -= dt * 2;
        }
        distance = SDL_clamp(distance, 2.5f, 8.0f);
        SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(gpu);
        if (!command_buffer) {
            goto done;
        }
        SDL_GPUTexture *swap = NULL;
        Uint32 sw;
        Uint32 sh;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, window, &swap, &sw, &sh)) {
            SDL_CancelGPUCommandBuffer(command_buffer);
            goto done;
        }
        if (!swap) {
            SDL_SubmitGPUCommandBuffer(command_buffer);
            SDL_Delay(10);
            continue;
        }
        /* The HUD texture refreshes twice a second, and every frame while the volume bar changes. */
        if (!hud_time || now - hud_time >= 500 || hk_system_overlay_dirty()) {
            SDL_SetRenderDrawColor(text_renderer, 0, 0, 0, 0);
            SDL_RenderClear(text_renderer);
            SDL_SetRenderScale(text_renderer, 3, 3);
            SDL_SetRenderDrawColor(text_renderer, 92, 231, 197, 255);
            SDL_RenderDebugText(text_renderer, 12, 12, "3/3 / VULKAN 3D");
            SDL_RenderDebugTextFormat(text_renderer, w / 3.0f - 95, 12, "FPS %.1f", fps);
            SDL_SetRenderScale(text_renderer, 2, 2);
            SDL_SetRenderDrawColor(text_renderer, 222, 232, 245, 255);
            SDL_RenderDebugText(text_renderer, 18, 40, "PowerVR GE8300 / depth-tested cube");
            SDL_RenderDebugTextFormat(text_renderer, 18, 55, "%s / L1-R1: zoom", paused ? "Paused" : "Rotating");
            SDL_RenderDebugText(text_renderer, 18, h / 2.0f - 32, "D-pad: rotate / A: pause / B: reset");
            SDL_RenderDebugText(text_renderer, 18, h / 2.0f - 18, "X-Y: cycle modes / Start + Select: exit");
            hk_system_draw(text_renderer); /* the volume bar, drawn into the HUD texture */
            SDL_RenderPresent(text_renderer);
            void *pixels = SDL_MapGPUTransferBuffer(gpu, upload, true);
            if (!pixels) {
                SDL_CancelGPUCommandBuffer(command_buffer);
                goto done;
            }
            for (Uint32 row = 0; row < h; row++) {
                SDL_memcpy((Uint8 *)pixels + row * w * 4, (Uint8 *)text->pixels + row * text->pitch, w * 4);
            }
            SDL_UnmapGPUTransferBuffer(gpu, upload);
            SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(command_buffer);
            SDL_UploadToGPUTexture(copy, &(SDL_GPUTextureTransferInfo){.transfer_buffer = upload, .pixels_per_row = w, .rows_per_layer = h}, &(SDL_GPUTextureRegion){.texture = hudtex, .w = w, .h = h, .d = 1}, true);
            SDL_EndGPUCopyPass(copy);
            hud_time = now;
        }
        SDL_GPUColorTargetInfo color_target = {.texture = color, .clear_color = {.r = .05f, .g = .078f, .b = .137f, .a = 1}, .load_op = SDL_GPU_LOADOP_CLEAR, .store_op = SDL_GPU_STOREOP_STORE, .cycle = true};
        SDL_GPUDepthStencilTargetInfo depth_target = {
            .texture = depth, .clear_depth = 1, .load_op = SDL_GPU_LOADOP_CLEAR, .store_op = SDL_GPU_STOREOP_DONT_CARE, .stencil_load_op = SDL_GPU_LOADOP_DONT_CARE, .stencil_store_op = SDL_GPU_STOREOP_DONT_CARE, .cycle = true};
        SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass(command_buffer, &color_target, 1, &depth_target);
        float view[] = {rx, ry, (float)w / h, distance};
        SDL_PushGPUVertexUniformData(command_buffer, 0, view, sizeof(view));
        SDL_BindGPUGraphicsPipeline(render_pass, cube);
        SDL_DrawGPUPrimitives(render_pass, 36, 1, 0, 0);
        SDL_BindGPUGraphicsPipeline(render_pass, hud);
        SDL_BindGPUFragmentSamplers(render_pass, 0, &(SDL_GPUTextureSamplerBinding){.texture = hudtex, .sampler = sampler}, 1);
        SDL_DrawGPUPrimitives(render_pass, 3, 1, 0, 0);
        SDL_EndGPURenderPass(render_pass);
        SDL_GPUBlitInfo blit = {.source = {.texture = color, .w = w, .h = h}, .destination = {.texture = swap, .w = sw, .h = sh}, .load_op = SDL_GPU_LOADOP_DONT_CARE, .filter = SDL_GPU_FILTER_NEAREST};
        SDL_BlitGPUTexture(command_buffer, &blit);
        bool take = capture && frames == capture_frame;
        if (take) {
            download = SDL_CreateGPUTransferBuffer(gpu, &(SDL_GPUTransferBufferCreateInfo){.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, .size = w * h * 4});
            if (!download) {
                SDL_CancelGPUCommandBuffer(command_buffer);
                goto done;
            }
            SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(command_buffer);
            SDL_DownloadFromGPUTexture(copy, &(SDL_GPUTextureRegion){.texture = color, .w = w, .h = h, .d = 1}, &(SDL_GPUTextureTransferInfo){.transfer_buffer = download, .pixels_per_row = w, .rows_per_layer = h});
            SDL_EndGPUCopyPass(copy);
            SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command_buffer);
            if (!fence) {
                goto done;
            }
            SDL_WaitForGPUFences(gpu, true, &fence, 1);
            SDL_ReleaseGPUFence(gpu, fence);
            void *pixels = SDL_MapGPUTransferBuffer(gpu, download, false);
            SDL_PixelFormat pf = format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM ? SDL_PIXELFORMAT_BGRA32 : SDL_PIXELFORMAT_RGBA32;
            SDL_Surface *shot = pixels ? SDL_CreateSurfaceFrom(w, h, pf, pixels, w * 4) : NULL;
            if (!shot || !SDL_SaveBMP(shot, capture)) {
                fprintf(stderr, "Capture failed: %s\n", SDL_GetError());
            } else {
                fprintf(stderr, "Vulkan output captured: %s\n", capture);
            }
            SDL_DestroySurface(shot);
            if (pixels) {
                SDL_UnmapGPUTransferBuffer(gpu, download);
            }
        } else if (!SDL_SubmitGPUCommandBuffer(command_buffer)) {
            goto done;
        }
        frames++;
        fps_frames++;
        if (now - fps_start >= 500) {
            fps = fps_frames * 1000.0f / (now - fps_start);
            fps_start = now;
            fps_frames = 0;
        }
    }
    fprintf(stderr, "Exit: frames=%u average_fps=%.1f\n", frames, frames * 1000.0 / (SDL_GetTicks() - start + 1));
    status = mode_request;
done:
    if (status == DEMO_ERROR) {
        fprintf(stderr, "Vulkan 3D error: %s\n", SDL_GetError());
    }
    hk_input_close(&input);
    hk_system_quit();
    SDL_DestroyRenderer(text_renderer);
    SDL_DestroySurface(text);
    if (gpu) {
        SDL_WaitForGPUIdle(gpu);
        if (cube) {
            SDL_ReleaseGPUGraphicsPipeline(gpu, cube);
        }
        if (hud) {
            SDL_ReleaseGPUGraphicsPipeline(gpu, hud);
        }
        if (cube_vertex_shader) {
            SDL_ReleaseGPUShader(gpu, cube_vertex_shader);
        }
        if (cube_fragment_shader) {
            SDL_ReleaseGPUShader(gpu, cube_fragment_shader);
        }
        if (hud_vertex_shader) {
            SDL_ReleaseGPUShader(gpu, hud_vertex_shader);
        }
        if (hud_fragment_shader) {
            SDL_ReleaseGPUShader(gpu, hud_fragment_shader);
        }
        if (color) {
            SDL_ReleaseGPUTexture(gpu, color);
        }
        if (depth) {
            SDL_ReleaseGPUTexture(gpu, depth);
        }
        if (hudtex) {
            SDL_ReleaseGPUTexture(gpu, hudtex);
        }
        if (upload) {
            SDL_ReleaseGPUTransferBuffer(gpu, upload);
        }
        if (download) {
            SDL_ReleaseGPUTransferBuffer(gpu, download);
        }
        if (sampler) {
            SDL_ReleaseGPUSampler(gpu, sampler);
        }
        if (claimed) {
            SDL_ReleaseWindowFromGPUDevice(gpu, window);
        }
        SDL_DestroyGPUDevice(gpu);
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    return status;
}
