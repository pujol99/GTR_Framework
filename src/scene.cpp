#include "scene.h"
#include "utils.h"

#include "prefab.h"
#include "extra/cJSON.h"

GTR::Scene* GTR::Scene::instance = NULL;

GTR::Scene::Scene()
{
	instance = this;
}

void GTR::Scene::clear()
{
	for (int i = 0; i < entities.size(); ++i)
	{
		BaseEntity* ent = entities[i];
		delete ent;
	}
	entities.resize(0);
}


void GTR::Scene::addEntity(BaseEntity* entity)
{
	entities.push_back(entity); entity->scene = this;
}

bool GTR::Scene::load(const char* filename)
{
	std::string content;

	this->filename = filename;
	std::cout << " + Reading scene JSON: " << filename << "..." << std::endl;

	if (!readFile(filename, content))
	{
		std::cout << "- ERROR: Scene file not found: " << filename << std::endl;
		return false;
	}

	//parse json string 
	cJSON* json = cJSON_Parse(content.c_str());
	if (!json)
	{
		std::cout << "ERROR: Scene JSON has errors: " << filename << std::endl;
		return false;
	}

	//read global properties
	background_color = readJSONVector3(json, "background_color", background_color);
	ambient_light = readJSONVector3(json, "ambient_light", ambient_light);

	//entities
	cJSON* entities_json = cJSON_GetObjectItemCaseSensitive(json, "entities");
	cJSON* entity_json;
	cJSON_ArrayForEach(entity_json, entities_json)
	{
		std::string type_str = cJSON_GetObjectItem(entity_json, "type")->valuestring;
		BaseEntity* ent = createEntity(type_str);
		if (!ent)
		{
			std::cout << " - ENTITY TYPE UNKNOWN: " << type_str << std::endl;
			//continue;
			ent = new BaseEntity();
		}

		addEntity(ent);

		if (cJSON_GetObjectItem(entity_json, "name"))
		{
			ent->name = cJSON_GetObjectItem(entity_json, "name")->valuestring;
			stdlog(std::string(" + entity: ") + ent->name);
		}

		//read transform
		if (cJSON_GetObjectItem(entity_json, "position"))
		{
			ent->model.setIdentity();
			Vector3 position = readJSONVector3(entity_json, "position", Vector3());
			ent->model.translate(position.x, position.y, position.z);
		}

		if (cJSON_GetObjectItem(entity_json, "angle"))
		{
			float angle = cJSON_GetObjectItem(entity_json, "angle")->valuedouble;
			ent->model.rotate(angle * DEG2RAD, Vector3(0, 1, 0));
		}

		if (cJSON_GetObjectItem(entity_json, "rotation"))
		{
			Vector4 rotation = readJSONVector4(entity_json, "rotation");
			Quaternion q(rotation.x, rotation.y, rotation.z, rotation.w);
			Matrix44 R;
			q.toMatrix(R);
			ent->model = R * ent->model;
		}

		if (cJSON_GetObjectItem(entity_json, "scale"))
		{
			Vector3 scale = readJSONVector3(entity_json, "scale", Vector3(1, 1, 1));
			ent->model.scale(scale.x, scale.y, scale.z);
		}

		ent->configure(entity_json);
	}

	//free memory
	cJSON_Delete(json);

	return true;
}

void GTR::Scene::setUniforms(Shader* shader, bool firstIteration) {
	if(firstIteration)
		shader->setUniform("u_ambient_light", ambient_light);
	else
		shader->setUniform("u_ambient_light", Vector3(0.0, 0.0, 0.0));

}

std::vector<GTR::LightEntity*> GTR::Scene::getLights(){
	std::vector<LightEntity*> lights;
	for (int i = 0; i < entities.size(); i++) {
		if (entities[i]->entity_type == LIGHT) {
			lights.push_back((LightEntity*) entities[i]);
		}
	}
	return lights;
}

GTR::BaseEntity* GTR::Scene::createEntity(std::string type)
{
	if (type == "PREFAB")
		return new GTR::PrefabEntity();
	if (type == "LIGHT")
		return new GTR::LightEntity();
    return NULL;
}

void GTR::BaseEntity::renderInMenu()
{
#ifndef SKIP_IMGUI
	ImGui::Text("Name: %s", name.c_str()); // Edit 3 floats representing a color
	ImGui::Checkbox("Visible", &visible); // Edit 3 floats representing a color
	//Model edit
	ImGuiMatrix44(model, "Model");
#endif
}

GTR::PrefabEntity::PrefabEntity()
{
	entity_type = PREFAB;
	prefab = NULL;
}

void GTR::PrefabEntity::configure(cJSON* json)
{
	if (cJSON_GetObjectItem(json, "filename"))
	{
		filename = cJSON_GetObjectItem(json, "filename")->valuestring;
		prefab = GTR::Prefab::Get( (std::string("data/") + filename).c_str());
	}
}

void GTR::PrefabEntity::renderInMenu()
{
	BaseEntity::renderInMenu();

#ifndef SKIP_IMGUI
	ImGui::Text("filename: %s", filename.c_str()); // Edit 3 floats representing a color
	if (prefab && ImGui::TreeNode(prefab, "Prefab Info"))
	{
		prefab->root.renderInMenu();
		ImGui::TreePop();
	}
#endif
}

GTR::LightEntity::LightEntity()
{
	entity_type = LIGHT;
	light = new Light();
}


void GTR::LightEntity::renderInMenu()
{
	BaseEntity::renderInMenu();

#ifndef SKIP_IMGUI
	if (light)
		light->renderInMenu();
#endif
}

void GTR::LightEntity::setLightType(std::string type) {
	if (type == "POINT")
		light_type = POINT;
	else if (type == "DIRECTIONAL")
		light_type = DIRECTIONAL;
	else if (type == "SPOT")
		light_type = SPOT;
}

void GTR::LightEntity::configure(cJSON* json)
{
	if (cJSON_GetObjectItem(json, "color"))
		light->light_color = readJSONVector3(json, "color", Vector3());
	if (cJSON_GetObjectItem(json, "intensity"))
		light->intensity = cJSON_GetObjectItem(json, "intensity")->valuedouble;
	if (cJSON_GetObjectItem(json, "max_distance"))
		light->max_distance = cJSON_GetObjectItem(json, "max_distance")->valuedouble; 
	if (cJSON_GetObjectItem(json, "cone_angle"))
		light->cone_angle = cJSON_GetObjectItem(json, "cone_angle")->valuedouble;
	if (cJSON_GetObjectItem(json, "direction")) {
		light->light_direction = readJSONVector3(json, "direction", Vector3());}
	if (cJSON_GetObjectItem(json, "lightType"))
		setLightType(cJSON_GetObjectItem(json, "lightType")->valuestring);
}

void GTR::LightEntity::setUniforms(Shader* shader, Camera* camera) {
	shader->setUniform("u_light_color", light->light_color);
	shader->setUniform("u_light_type", light_type);
	shader->setUniform("u_light_intensity", light->intensity);
	shader->setUniform("u_light_cone_angle", light->cone_angle);
	shader->setUniform("u_light_max_distance", light->max_distance);
	shader->setUniform("u_light_position", model.getTranslation());
	shader->setUniform("u_light_direction", light->light_direction);


}

