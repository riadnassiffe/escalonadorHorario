from fastapi import APIRouter

from app.api.v1 import auth, config, entities, jobs, projects, timetable

api_router = APIRouter()


@api_router.get("/health", tags=["health"])
async def health_check():
    return {"status": "ok"}


api_router.include_router(auth.router)
api_router.include_router(projects.router)
api_router.include_router(entities.router)
api_router.include_router(config.router)
api_router.include_router(timetable.router)
api_router.include_router(jobs.router)
