/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "app/Config.h"
#include "app/FilamentApp.h"
#include "app/MeshAssimp.h"

#include <filagltf/GltfLoader.h>

#include <filament/Engine.h>
#include <filament/LightManager.h>
#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <filament/RenderableManager.h>
#include <filament/TransformManager.h>
#include <filament/Scene.h>
#include <filament/Texture.h>
#include <filament/IndirectLight.h>

#include "generated/resources/resources.h"

#include <utils/Path.h>
#include <utils/EntityManager.h>

#include <math/mat3.h>
#include <math/mat4.h>
#include <math/vec3.h>

#include <getopt/getopt.h>

#include <stb_image.h>

#include <memory>
#include <map>
#include <string>
#include <vector>

using namespace math;
using namespace filament;
using namespace filamat;
using namespace utils;

static std::vector<Path> g_filenames;

static std::map<std::string, MaterialInstance*> g_materialInstances;
static std::unique_ptr<MeshAssimp> g_meshSet;
static const Material* g_material;
static Entity g_light;

static Config g_config;

static void printUsage(char* name) {
    std::string usage(
            "gltf_viewer displays gltf models using the filament renderer\n"
            "Usage:\n"
            "    gltf_viewer [options] <gltf/glb>\n"
            "Options:\n"
            "   --help, -h\n"
            "       Prints this message\n\n"
            "   --ibl=<path to cmgen IBL>, -i <path>\n"
            "       Applies an IBL generated by cmgen's deploy option\n\n"
            "   --split-view, -v\n"
            "       Splits the window into 4 views\n\n"
    );
    std::cout << usage;
}

static int handleCommandLineArgments(int argc, char* argv[], Config* config) {
    static constexpr const char* OPTSTR = "hi:v";
    static const struct option OPTIONS[] = {
            { "help",           no_argument,       nullptr, 'h' },
            { "ibl",            required_argument, nullptr, 'i' },
            { "split-view",     no_argument,       nullptr, 'v' },
            { 0, 0, 0, 0 }  // termination of the option list
    };
    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, OPTSTR, OPTIONS, &option_index)) >= 0) {
        std::string arg(optarg ? optarg : "");
        switch (opt) {
            default:
            case 'h':
                printUsage(argv[0]);
                exit(0);
            case 'i':
                config->iblDirectory = arg;
                break;
            case 'v':
                config->splitView = true;
                break;
        }
    }

    return optind;
}

static void cleanup(Engine* engine, View* view, Scene* scene) {
    for (auto& item : g_materialInstances) {
        auto materialInstance = item.second;
        engine->destroy(materialInstance);
    }
    g_meshSet.reset(nullptr);
    engine->destroy(g_material);

    EntityManager& em = EntityManager::get();
    engine->destroy(g_light);
    em.destroy(g_light);
}

static void setup(Engine* engine, View* view, Scene* scene) {
    Material *defaultMaterial = Material::Builder()
            .package(RESOURCES_AIDEFAULTMAT_DATA, RESOURCES_AIDEFAULTMAT_SIZE)
            .build(*engine);

    defaultMaterial->setDefaultParameter("baseColor",   RgbType::LINEAR, float3{0.8});
    defaultMaterial->setDefaultParameter("metallic",    0.0f);
    defaultMaterial->setDefaultParameter("roughness",   0.4f);
    defaultMaterial->setDefaultParameter("reflectance", 0.5f);

    GltfLoader loader = GltfLoader(*engine, defaultMaterial);

    for (auto& filename : g_filenames) {
        loader.Load(filename);
    }

    g_light = EntityManager::get().create();
    LightManager::Builder(LightManager::Type::SUN)
            .color(Color::toLinear<ACCURATE>(sRGBColor(0.98f, 0.92f, 0.89f)))
            .intensity(110000)
            .direction({ 0.7, -1, -0.8 })
            .sunAngularRadius(1.9f)
            .castShadows(true)
            .build(*engine, g_light);
    scene->addEntity(g_light);
}

int main(int argc, char* argv[]) {
    int option_index = handleCommandLineArgments(argc, argv, &g_config);
    int num_args = argc - option_index;
    if (num_args < 1) {
        printUsage(argv[0]);
        return 1;
    }

    for (int i = option_index; i < argc; i++) {
        utils::Path filename = argv[i];
        if (!filename.exists()) {
            std::cerr << "file " << argv[option_index] << " not found!" << std::endl;
            return 1;
        }
        g_filenames.push_back(filename);
    }

    FilamentApp& filamentApp = FilamentApp::get();
    filamentApp.run(g_config, setup, cleanup);

    return 0;
}