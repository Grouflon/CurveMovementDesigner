// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

class UCurveFloat;

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "CurvePawn.generated.h"

UCLASS()
class CURVEACCELERATION_API ACurvePawn : public APawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	ACurvePawn();

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual FVector GetVelocity() const override;

	virtual void DisplayDebug(class UCanvas* Canvas, const class FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) override;

private:
	void _OnHorizontalAxis(float _value);

	UPROPERTY(EditAnywhere) float m_maxVelocity = 100.f;
	UPROPERTY(EditAnywhere) float m_velocityChangeThreshold = 0.1f;
	UPROPERTY(EditAnywhere) UCurveFloat* m_accelerationCurve = nullptr;
	UPROPERTY(EditAnywhere) UCurveFloat* m_decelerationCurve = nullptr;

	enum EAccelerationState
	{
		AS_Stable,
		AS_Accelerating,
		AS_Decelerating
	};

	EAccelerationState m_accelerationState = AS_Stable;
	UCurveFloat* m_currentCurve = nullptr;
	float m_currentTimeOnCurve = 0.f;
	float m_input = 0.f;
	float m_previousInput = 0.f;
	FVector m_velocity = FVector::ZeroVector;
};
