#ifndef SCENE_H
#define SCENE_H

#include "framework.h"
#include "shader.h"
#include "camera.h"
#include <string>

//forward declaration
class cJSON; 
class Shader;


//our namespace
namespace GTR {

	enum eEntityType {
		NONE = 0,
		PREFAB = 1,
		LIGHT = 2,
		CAMERA = 3,
		REFLECTION_PROBE = 4,
		DECALL = 5
	};

	enum eLightType { 
		DIRECTIONAL = 0, 
		POINT = 1, 
		SPOT = 2
	};

	class Scene;
	class Prefab;
	class Light;

	//represents one element of the scene (could be lights, prefabs, cameras, etc)
	class BaseEntity
	{
	public:
		Scene* scene;
		std::string name;
		eEntityType entity_type;
		Matrix44 model;
		bool visible;
		BaseEntity() { entity_type = NONE; visible = true; }
		virtual ~BaseEntity() {}
		virtual void renderInMenu();
		virtual void configure(cJSON* json) {}
	};

	//represents one prefab in the scene
	class PrefabEntity : public GTR::BaseEntity
	{
	public:
		std::string filename;
		Prefab* prefab;
		
		PrefabEntity();

		virtual void renderInMenu();
		virtual void configure(cJSON* json);
	};


	class LightEntity : public GTR::BaseEntity
	{
	public:
		Light* light;
		eLightType light_type;

		LightEntity();

		virtual void renderInMenu();
		virtual void configure(cJSON* json);
		void setUniforms(Shader* shader, Camera* camera);
		void setLightType(std::string type);
	};
	//contains all entities of the scene
	class Scene
	{
	public:
		static Scene* instance;

		Vector3 background_color;
		Vector3 ambient_light;

		Scene();

		std::string filename;
		std::vector<BaseEntity*> entities;

		void clear();
		void addEntity(BaseEntity* entity);

		bool load(const char* filename);
		BaseEntity* createEntity(std::string type);
		std::vector<LightEntity*> getLights();
		void setUniforms(Shader* shader, bool firstIteration);
	};

};

#endif