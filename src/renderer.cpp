#include "renderer.h"

#include "camera.h"
#include "shader.h"
#include "mesh.h"
#include "texture.h"
#include "prefab.h"
#include "material.h"
#include "utils.h"
#include "scene.h"
#include "extra/hdre.h"
#include <algorithm>


using namespace GTR;

RenderCall::RenderCall(const Matrix44 model, GTR::Node* node, float distanceCamera) {
	this->model = model;
	this->node = node;

	this->distanceCamera = distanceCamera;
	this->isTransparent = (this->node->material->alpha_mode == GTR::eAlphaMode::BLEND);
}

void Renderer::renderScene(GTR::Scene* scene, Camera* camera)
{
	//set the clear color (the background color)
	glClearColor(scene->background_color.x, scene->background_color.y, scene->background_color.z, 1.0);

	// Clear the color and the depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	checkGLErrors();

	//render entities
	for (int i = 0; i < scene->entities.size(); ++i)
	{
		BaseEntity* ent = scene->entities[i];
		if (!ent->visible)
			continue;

		//is a prefab!
		if (ent->entity_type == PREFAB)
		{
			PrefabEntity* pent = (GTR::PrefabEntity*)ent;
			if(pent->prefab)
				renderPrefab(ent->model, pent->prefab, camera);
		}
	}
	sortCalls();
	renderCalls(camera);
}

//renders all the prefab
void Renderer::renderPrefab(const Matrix44& model, GTR::Prefab* prefab, Camera* camera)
{
	assert(prefab && "PREFAB IS NULL");
	//assign the model to the root node
	renderNode(model, &prefab->root, camera);
}

//renders a node of the prefab and its children
void Renderer::renderNode(const Matrix44& prefab_model, GTR::Node* node, Camera* camera)
{
	if (!node->visible)
		return;

	//compute global matrix
	Matrix44 node_model = node->getGlobalMatrix(true) * prefab_model;

	//does this node have a mesh? then we must render it
	if (node->mesh && node->material)
	{
		//compute the bounding box of the object in world space (by using the mesh bounding box transformed to world space)
		BoundingBox world_bounding = transformBoundingBox(node_model,node->mesh->box);
		
		//if bounding box is inside the camera frustum then the object is probably visible
		if (camera->testBoxInFrustum(world_bounding.center, world_bounding.halfsize) )
		{
			calls.push_back(new RenderCall( 
				node_model, 
				node, 
				node_model.getTranslation().distance(camera->eye)));
			//node->mesh->renderBounding(node_model, true);
		}
	}

	//iterate recursively with children
	for (int i = 0; i < node->children.size(); ++i)
		renderNode(prefab_model, node->children[i], camera);
}

bool initRender(GTR::Material* material, Mesh* mesh) {
	//in case there is nothing to do
	if (!mesh || !mesh->getNumVertices() || !material)
		return false;
	assert(glGetError() == GL_NO_ERROR);

	//select the blending
	if (material->alpha_mode == GTR::eAlphaMode::BLEND) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
		glDisable(GL_BLEND);

	//select if render both sides of the triangles
	if (material->two_sided)
		glDisable(GL_CULL_FACE);
	else
		glEnable(GL_CULL_FACE);

	assert(glGetError() == GL_NO_ERROR);
	return true;
}

void loadTextures(Shader* shader, GTR::Material* material) {
	Texture* texture = material->color_texture.texture;
	if (!texture)
		texture = Texture::getWhiteTexture();
	Texture* emissive_texture = material->emissive_texture.texture;
	if (!emissive_texture)
		emissive_texture = Texture::getWhiteTexture();
	Texture* occlusion_texture = material->occlusion_texture.texture;
	if (!occlusion_texture)
		occlusion_texture = Texture::getWhiteTexture();
	Texture* metallic_roughness_texture = material->metallic_roughness_texture.texture;
	if (!metallic_roughness_texture)
		metallic_roughness_texture = Texture::getWhiteTexture();

	shader->enable();
	shader->setUniform("u_texture", texture, 0);
	shader->setUniform("u_emissive_texture", emissive_texture, 1);
	shader->setUniform("u_occlusion_texture", occlusion_texture, 2);
	shader->setUniform("u_metallic_roughness_texture", metallic_roughness_texture, 3);
	shader->setUniform("u_emissive_factor", material->emissive_factor);
}

//renders a mesh given its transform and material
void Renderer::renderMeshWithMaterial(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera)
{
	if (!initRender(material, mesh))
		return;

	float time = GetTickCount64();
	Shader* shader = Shader::Get("phong");
	if (!shader)
		return;
	
	loadTextures(shader, material);
	camera->setUniforms(shader);
	shader->setUniform("u_model", model);
	shader->setUniform("u_time", time);
	material->setUniforms(shader);
	


	std::vector< LightEntity*> lights = GTR::Scene::instance->getLights();
	for (int i = 0; i < lights.size(); i++) {
		if (i == 0)
			GTR::Scene::instance->setUniforms(shader, true);
		else {
			glDepthFunc(GL_LEQUAL);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			glEnable(GL_BLEND);
			GTR::Scene::instance->setUniforms(shader, false);
		}
		lights[i]->setUniforms(shader, camera);

		//do the draw call that renders the mesh into the screen
		mesh->render(GL_TRIANGLES);
	}

	shader->disable();
	glDisable(GL_BLEND);
	glDepthFunc(GL_LESS);
	glDisable(GL_BLEND);
}

void Renderer::renderCalls(Camera* camera) {
	for (int i = 0; i < calls.size(); i++)
		renderMeshWithMaterial(calls[i]->model, calls[i]->node->mesh, calls[i]->node->material, camera);
	calls.clear();
}

void Renderer::sortCalls() {
	std::sort(calls.begin(), calls.end(), comparator);
}


Texture* GTR::CubemapFromHDRE(const char* filename)
{
	HDRE* hdre = new HDRE();
	if (!hdre->load(filename))
	{
		delete hdre;
		return NULL;
	}

	/*
	Texture* texture = new Texture();
	texture->createCubemap(hdre->width, hdre->height, (Uint8**)hdre->getFaces(0), hdre->header.numChannels == 3 ? GL_RGB : GL_RGBA, GL_FLOAT );
	for(int i = 1; i < 6; ++i)
		texture->uploadCubemap(texture->format, texture->type, false, (Uint8**)hdre->getFaces(i), GL_RGBA32F, i);
	return texture;
	*/
	return NULL;
}