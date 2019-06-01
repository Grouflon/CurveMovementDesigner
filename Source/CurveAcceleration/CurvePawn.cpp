// Fill out your copyright notice in the Description page of Project Settings.

#include "CurvePawn.h"
#include <Components/InputComponent.h>
#include <Curves/CurveFloat.h>
#include <Draw.h>
#include <Log.h>

// Sets default values
ACurvePawn::ACurvePawn()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ACurvePawn::BeginPlay()
{
	Super::BeginPlay();
	
	FHitResult hit;
	if (GetWorld()->LineTraceSingleByChannel(hit, GetActorLocation(), GetActorLocation() + FVector(0.f, 0.f, -1000.f), ECC_Visibility))
	{
		SetActorLocation(hit.ImpactPoint);
	}
}

bool FindTimeOnCurveForValue(UCurveFloat* _curve, float _value, float* _outTime, uint16 _maxSamplesCount = 128)
{
	if (_curve->GetCurves().Num() == 0)
		return false;

	float firstKeyTime = _curve->GetCurves()[0].CurveToEdit->GetFirstKey().Time;
	float lastKeyTime = _curve->GetCurves()[0].CurveToEdit->GetLastKey().Time;

	float samplingInterval = (lastKeyTime - firstKeyTime) / float(_maxSamplesCount);
	if (samplingInterval == 0.f)
	{
		if (_outTime) *_outTime = _curve->GetFloatValue(firstKeyTime);
		return true;
	}

	bool found = false;
	float outTime = 0.f;
	for (uint16 i = 0; i < _maxSamplesCount; ++i)
	{
		float currentTime = float(i) * samplingInterval;
		float nextTime = float(i + 1) * samplingInterval;
		float currentSample = _curve->GetFloatValue(currentTime);
		float nextSample = _curve->GetFloatValue(nextTime);

		float minSample = FMath::Min(currentSample, nextSample);
		float maxSample = FMath::Max(currentSample, nextSample);

		if (_value >= minSample && _value <= maxSample)
		{
			float interval = maxSample - minSample;
			if (FMath::IsNearlyZero(interval))
			{
				outTime = minSample;
			}
			else
			{
				float t = (_value - minSample) / interval;
				if (nextSample < currentSample) t = 1.f - t;
				outTime = currentTime + t * samplingInterval;
			}
			found = true;
			break;
		}
	}

	if (_outTime) *_outTime = outTime;
	return found;
}

// Called every frame
void ACurvePawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!m_accelerationCurve || !m_decelerationCurve)
		return;

	float wantedVelocity = FMath::Abs(m_input * m_maxVelocity);
	FVector currentVelocityVector = GetVelocity();
	float currentVelocity = currentVelocityVector.Size();

	{
		FVector wantedVelocityVector = GetActorForwardVector() * m_input * m_maxVelocity;
		float currentVelocityRatio = FMath::Clamp(currentVelocity / m_maxVelocity, 0.f, 1.f);
		float velocityDiff = wantedVelocity - currentVelocity;
		bool changingDirection = FVector::DotProduct(currentVelocityVector, wantedVelocityVector) < 0.f;
		if (velocityDiff < -m_velocityChangeThreshold || changingDirection)
		{
			if (changingDirection)
			{
				wantedVelocity = 0.f;
				wantedVelocityVector = FVector::ZeroVector;
			}
			m_currentCurve = m_decelerationCurve;
			m_accelerationState = AS_Decelerating;
			bool result = FindTimeOnCurveForValue(m_decelerationCurve, currentVelocityRatio, &m_currentTimeOnCurve);
			ensure(result);
		}
		else if (FMath::IsNearlyZero(velocityDiff, m_velocityChangeThreshold))
		{
			m_accelerationState = AS_Stable;
		}
		else
		{
			m_currentCurve = m_accelerationCurve;
			m_accelerationState = AS_Accelerating;
			bool result = FindTimeOnCurveForValue(m_accelerationCurve, currentVelocityRatio, &m_currentTimeOnCurve);
			ensure(result);
		}
	}

	// COMPUTE VELOCITY
	float ratio;
	float velocity;
	if (m_accelerationState == AS_Stable)
	{
		ratio = m_input;
		velocity = m_input * m_maxVelocity;
	}
	else
	{
		m_currentTimeOnCurve += DeltaTime;

		ratio = m_currentCurve->GetFloatValue(m_currentTimeOnCurve);
		velocity = ratio * m_maxVelocity;


		if ((m_accelerationState == AS_Accelerating && velocity > wantedVelocity) || (m_accelerationState == AS_Decelerating && velocity < wantedVelocity))
		{
			velocity = wantedVelocity;
			bool result = FindTimeOnCurveForValue(m_currentCurve, FMath::Abs(m_input), &m_currentTimeOnCurve);
			ensure(result);
		}

		float sign = FMath::Sign(FVector::DotProduct(GetActorForwardVector(), currentVelocityVector));
		if (sign == 0.f)
		{
			sign = FMath::Sign(m_input);
		}
		velocity *= sign;
	}

	LOGF_SCREEN(0.f, FColor::Cyan, "%d/%.2f/%.2f/%.2f/%.2f", m_accelerationState, m_input, m_currentTimeOnCurve, ratio, velocity);

	// CHANGE LOCATION
	FVector previousLocation = GetActorLocation();
	FVector newLocation = previousLocation;
	newLocation.X += velocity * DeltaTime;
	SetActorLocation(newLocation);
	m_velocity = (newLocation - previousLocation) / DeltaTime;
	
	m_previousInput = m_input;
}

// Called to bind functionality to input
void ACurvePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis("Horizontal", this, &ACurvePawn::_OnHorizontalAxis);
}

FVector ACurvePawn::GetVelocity() const
{
	return m_velocity;
}

void ACurvePawn::DisplayDebug(class UCanvas* Canvas, const class FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	Super::DisplayDebug(Canvas, DebugDisplay, YL, YPos);

	if (m_currentCurve)
	{
		float firstKeyTime = m_currentCurve->GetCurves()[0].CurveToEdit->GetFirstKey().Time;
		float lastKeyTime = m_currentCurve->GetCurves()[0].CurveToEdit->GetLastKey().Time;
		DrawDebugCanvasCurve(Canvas, m_currentCurve, FBox2D(FVector2D(50.f, 210.f), FVector2D(250.f, 310.f)), firstKeyTime, lastKeyTime, m_currentTimeOnCurve, m_currentCurve->GetName());
	}
}

void ACurvePawn::_OnHorizontalAxis(float _value)
{
	m_input = _value;
}

